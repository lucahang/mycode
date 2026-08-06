#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <stdexcept>
#include <cstring>

class Epoll{
public:
    explicit Epoll(int max_events = 64,int flags=0)
        :epfd_(::epoll_create1(flags)),max_events_(max_events){
        if (epfd_ == -1) {
            throw std::runtime_error("epoll_create1 failed: " + std::string(strerror(errno)));
        }
        events_.resize(max_events_);
    }
    // 禁止拷贝
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    ~Epoll(){
        if (epfd_ != -1) {
            ::close(epfd_);
        }
    }
    void add_fd(int fd, uint32_t events = EPOLLIN, bool edge_triggered = false){
        struct epoll_event ev;
        ev.events=events;
        if(edge_triggered){
            ev.events|=EPOLLET;
        }
        ev.data.fd = fd;
        if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw std::runtime_error("epoll_ctl ADD failed: " + std::string(strerror(errno)));
        }
    }
    void mod_fd(int fd, uint32_t events, bool edge_triggered = false){
        struct epoll_event ev;
        ev.events=events;
        if(edge_triggered){
            ev.events|=EPOLLET;
        }
        ev.data.fd=fd;
        if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
            throw std::runtime_error("epoll_ctl MOD failed: " + std::string(strerror(errno)));
        }
    }
    void del_fd(int fd){
        if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            throw std::runtime_error("epoll_ctl DEL failed: " + std::string(strerror(errno)));
        }
    }
    std::vector<struct epoll_event> wait(int timeout_ms=-1){
        int nfds = ::epoll_wait(epfd_,events_.data(),max_events_,timeout_ms);
        if(nfds==-1){
            if (errno == EINTR) {
                // 被信号中断，返回空列表
                return {};
            }
            throw std::runtime_error("epoll_wait failed: " + std::string(strerror(errno)));
        }
        return std::vector<struct epoll_event>(events_.begin(), events_.begin() + nfds);
    }
    int fd() const { return epfd_; }
private:
    int epfd_;
    int max_events_;
    std::vector<struct epoll_event> events_;
};


int main() {
    // 1. 创建监听 socket
    Socket ser_socket;
    ser_socket.set_reuseaddr();
    ser_socket.createTCP();
    ser_socket.set_nonblocking();
    if (!ser_socket.valid()) {
        perror("socket");
        return 1;
    }
    ser_socket.bind(8888,"0.0.0.0");
    if (ser_socket.listen(128) < 0) {
        perror("listen");
        return 1;
    }
    // 2. 创建 Epoll 对象，最大事件数 64
    Epoll epoll(64);

    // 3. 将监听 fd 加入 epoll，边缘触发
    epoll.add_fd(ser_socket.fd(), EPOLLIN, true);

    std::cout << "Echo server running on port 8888" << std::endl;

    // 4. 事件循环
    while (true) {
        auto events = epoll.wait(-1);  // 阻塞等待
        for (const auto& ev : events) {
            int fd = ev.data.fd;
            if (fd == ser_socket.fd()) {
                // 接受新连接
                while (true) {
                    int client_fd = accept4(ser_socket.fd(), nullptr, nullptr, SOCK_NONBLOCK);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    // 加入 epoll，监听可读事件
                    epoll.add_fd(client_fd, EPOLLIN | EPOLLRDHUP, true);
                    std::cout << "New client: " << client_fd << std::endl;
                }
            } else {
                // 先处理错误/挂起事件
                if (ev.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    epoll.del_fd(fd);
                    close(fd);
                    std::cout << "Client " << fd << " closed (hup/err)" << std::endl;
                    continue;  // 跳过后续处理
                }
                // 处理客户端数据
                if (ev.events & EPOLLIN) {
                    char buffer[1024]={0};
                    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
                    if (n <= 0) {
                        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // 数据已读完，继续等待下次事件
                            continue;
                        }
                        // 连接关闭或错误
                        epoll.del_fd(fd);
                        close(fd);
                        std::cout << "Client " << fd << " disconnected" << std::endl;
                    } else {
                        // 回显
                        send(fd, buffer, n, 0);
                        std::cout<<"收到客户端消息("<<fd<<"): "<<buffer<<std::endl;
                    }
                }
                if (ev.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    epoll.del_fd(fd);
                    close(fd);
                    std::cout << "Client " << fd << " closed (hup/err)" << std::endl;
                }
            }
        }
    }
    return 0;
}