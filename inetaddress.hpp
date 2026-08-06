#pragma once

#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h> 
#include <unistd.h>

class InetAddress{
public:
    InetAddress(){
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_.sin_port = 0;
    }
     // 通过 IP 字符串和端口构造
    explicit InetAddress(uint16_t port, const std::string& ip = "0.0.0.0"){
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
            // 解析失败，设为 INADDR_ANY
            addr_.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }

    // 通过 sockaddr_in 构造
    explicit InetAddress(const struct sockaddr_in& addr): addr_(addr) {}

    // 获取 IP 字符串
    std::string toIp() const{
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf))) {
            return std::string(buf);
        }
        return "0.0.0.0";
    }

    // 获取端口（主机字节序）
    uint16_t toPort() const{
        return ntohs(addr_.sin_port);
    }

    // 获取原生 sockaddr_in 指针（用于系统调用）
    const struct sockaddr* getSockAddr() const{
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    struct sockaddr* getSockAddr(){
        return reinterpret_cast<struct sockaddr*>(&addr_);
    }

    // 获取结构体大小
    socklen_t getSockLen() const{
        return sizeof(addr_);
    }

    // 设置 IP 和端口
    void setIp(const std::string& ip){
        if (inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
            addr_.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }
    void setPort(uint16_t port){
        addr_.sin_port=htons(port);
    }

    // 域名解析：返回解析后的 IP 字符串（第一个有效地址），失败返回空字符串
    std::string resolve(const std::string& hostname) {
        struct addrinfo hints, *res, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;      // 只解析 IPv4
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
        if (status != 0) {
            return "";   // 解析失败
        }

        std::string ip;
        for (p = res; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
                char buf[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &addr_in->sin_addr, buf, sizeof(buf))) {
                    ip = buf;
                    break;
                }
            }
        }
        freeaddrinfo(res);
        return ip;
    }
private:
    struct sockaddr_in addr_;
};