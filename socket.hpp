#pragma once

#include <errno.h>
#include <sys/socket.h>
#include <cstdint>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>  

class Socket{
public:
    Socket():fd_(-1){}
    explicit Socket(int fd):fd_(fd){}
    ~Socket(){close();}

    Socket(const Socket& )=delete;
    Socket& operator=(const Socket& )=delete;

    Socket(Socket&& other)noexcept{
        fd_=other.fd_;
        other.fd_=-1;
    }
    Socket& operator=(Socket&& other)noexcept{
        if(this != &other){
            close();
            fd_=other.fd_;
            other.fd_=-1;
        }
        return *this;
    }
    bool createTCP() {
        close(); // 确保之前没有打开
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        return fd_ != -1;
    }
    bool bind(uint16_t port, const std::string& ip = "0.0.0.0"){
        if(fd_==-1) return false;
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);    
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
        return ::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    }   
    bool listen(int backlog = 128) {
        if (fd_ == -1) return false;
        return ::listen(fd_, backlog) == 0;
    }
    Socket accept(){
        if(fd_==-1)return Socket();
        struct sockaddr_in client_addr;
        socklen_t len=sizeof(client_addr);
        int client_fd=::accept(fd_,(sockaddr*) &client_addr,&len);
        return Socket(client_fd);
    }
    bool connect(const std::string& ip,uint16_t port){
        if(fd_==-1) return false;
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family=AF_INET;
        addr.sin_port=htons(port);
        if(inet_pton(AF_INET,ip.c_str(),&addr.sin_addr)<=0){
            return false;
        }
        return ::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    }
    // ---------- 发送数据（带 MSG_NOSIGNAL 防止 SIGPIPE） ----------
    int send(const char* data, size_t len, int flags = MSG_NOSIGNAL) {
        if (fd_ == -1) return -1;
        return ::send(fd_, data, len, flags);
    }
    int recv(char* buffer, size_t len, int flags = 0) {
        if (fd_ == -1) return -1;
        return ::recv(fd_, buffer, len, flags);
    }
    //can't be use in non-blocking mode
    bool send_all(const char* data, size_t len){
        if(fd_==-1) return false;
        size_t total=0;
        while(total<len){
            ssize_t n=::send(fd_,data+total,len-total,MSG_NOSIGNAL);
            if(n<=0){
                if(n<0 && errno == EINTR) continue;
                return false;
            }
            total+=n;
        }
        return true;
    }
    //can't be use in non-blocking mode
    int recv_all(char* buffer, size_t len){
        if(fd_==-1) return -1;
        size_t total=0;
        while(total<len){
            ssize_t n=::recv(fd_,buffer,len-total,0);
            if(n==0){
                break;
            }
            else if(n<0){
                if(errno == EINTR) continue;
                return -1;
            }
            total+=n;
        }
        return static_cast<int>(total);
    }
    void close(){
        if(fd_!=-1){
            ::close(fd_);
            fd_=-1;
        }
    }
    // ---------- 设置非阻塞模式 ----------
    bool set_nonblocking(bool enable = true) {
        if (fd_ == -1) return false;
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags == -1) return false;
        if (enable)
            flags |= O_NONBLOCK;
        else
            flags &= ~O_NONBLOCK;
        return fcntl(fd_, F_SETFL, flags) == 0;
    }
    // ---------- 设置 SO_REUSEADDR（端口复用） ----------
    bool set_reuseaddr(bool enable = true) {
        if (fd_ == -1) return false;
        int opt = enable ? 1 : 0;
        return setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
    }
    int fd()const{return fd_;}
    bool valid()const{return -1!=fd_;}
    static int last_error() {
        return errno;
    }
private:    
    int fd_;
};
//client 
int main() {
    Socket client;

    // 1. 创建 TCP Socket
    if (!client.createTCP()) {
        std::cerr << "创建 Socket 失败！" << std::endl;
        return -1;
    }

    // 2. 连接服务端 (如本机 127.0.0.1:8080)
    std::cout << "正在连接服务器..." << std::endl;
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "连接服务器失败，错误码: " << Socket::last_error() << std::endl;
        return -1;
    }

    // 3. 发送数据
    std::string msg = "Hello Server, this is Client!";
    if (client.send_all(msg.c_str(), msg.size())) {
        std::cout << "发送消息成功！" << std::endl;
    }

    // 4. 接收服务端回复
    char buffer[1024] = {0};
    int bytes_read = client.recv(buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        std::cout << "收到服务端回复: " << buffer << std::endl;
    }

    return 0;
}
//server
int main() {
    Socket server;
    
    // 1. 创建 TCP Socket
    if (!server.createTCP()) {
        std::cerr << "创建 Socket 失败，错误码: " << Socket::last_error() << std::endl;
        return -1;
    }

    // 2. 设置端口复用（防止服务器重启时报 Address already in use 错误）
    server.set_reuseaddr(true);

    // 3. 绑定 IP 和端口 (监听 0.0.0.0:8080)
    if (!server.bind(8080, "0.0.0.0")) {
        std::cerr << "绑定端口失败，错误码: " << Socket::last_error() << std::endl;
        return -1;
    }

    // 4. 开始监听
    if (!server.listen()) {
        std::cerr << "监听失败，错误码: " << Socket::last_error() << std::endl;
        return -1;
    }

    std::cout << "服务器启动，正在监听端口 8080..." << std::endl;

    // 5. 接收客户端连接
    Socket client = server.accept();
    if (client.valid()) {
        std::cout << "客户端成功连接！" << std::endl;

        // 6. 接收客户端发来的数据
        char buffer[1024] = {0};
        int bytes_read = client.recv(buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            std::cout << "收到客户端消息: " << buffer << std::endl;

            // 7. 给客户端回复数据
            std::string reply = "Hello from Server!";
            client.send_all(reply.c_str(), reply.size());
        }
    }

    // 8. 离开作用域时，client 和 server 会通过析构函数自动 close()
    return 0;
}