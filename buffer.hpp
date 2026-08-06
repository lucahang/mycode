#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

class Buffer{
public:
    static const size_t kInitialSize = 1024;
    static const size_t kCheapPrepend = 8;  // 预留头部空间（可选）

    Buffer(size_t initial_size = kInitialSize)
        : buffer_(initial_size+kCheapPrepend),
        readIndex_(kCheapPrepend),
        writeIndex_(kCheapPrepend) {
    }

    // 可读数据大小
    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    // 可写空间大小
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    // 预留空间（前面可回收空间，用于移动数据）
    size_t prependableBytes() const { return readIndex_; }

    // 返回可读数据起始地址
    const char* peek() const { return begin() + readIndex_; }
    char* beginWrite() { return begin() + writeIndex_; }
    const char* beginWrite() const { return begin() + writeIndex_; }

    // 移动读指针（取出数据）
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readIndex_ += len;
        } else {
            retrieveAll();
        }
    }

    void retrieveAll() {
        readIndex_ = kCheapPrepend;
        writeIndex_ = kCheapPrepend;
    }

    // 取出数据并返回为 std::string
    std::string retrieveAsString(size_t len) {
        if (len > readableBytes()) len = readableBytes();
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    // 追加数据（从内存）
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writeIndex_ += len;
    }

    void append(const std::string& str) {
        append(str.c_str(), str.size());
    }

    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            // 尝试移动已有数据到前部（回收 prepend 空间）
            if (prependableBytes() + writableBytes() < len + kCheapPrepend) {
                // 空间仍不足，扩容
                size_t new_size = buffer_.size() * 2;
                while (new_size < writeIndex_ + len + kCheapPrepend) {
                    new_size *= 2;
                }
                buffer_.resize(new_size);
            }
            // 移动数据到开头（保留 kCheapPrepend 空间）
            moveDataToFront();
        }
    }

    // 从文件描述符读取数据（非阻塞，边缘触发时循环读取）
    ssize_t readFd(int fd, int* savedErrno = nullptr) {
        // 使用栈上临时缓冲区，配合 readv 分散读，减少内存拷贝
        char extrabuf[65536];
        struct iovec vec[2];
        const size_t writable = writableBytes();
        vec[0].iov_base = beginWrite();
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);

        const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
        const ssize_t n = ::readv(fd, vec, iovcnt);
        if (n < 0) {
            if (savedErrno) *savedErrno = errno;
            return n;
        } else if (static_cast<size_t>(n) <= writable) {
            writeIndex_ += n;
        } else {
            writeIndex_ = buffer_.size();
            append(extrabuf, n - writable);
        }
        return n;
    }
    // 向文件描述符写入数据（非阻塞发送尽可能多的数据）
    ssize_t writeFd(int fd, int* savedErrno = nullptr) {
        ssize_t n = ::write(fd, peek(), readableBytes());
        if (n < 0) {
            if (savedErrno) *savedErrno = errno;
            return n;
        }
        retrieve(n);
        return n;
    }
    // 查找特定字符（如 '\n' 或 '\r\n'）
    const char* findCRLF() const {
        const char* start = peek();
        const char* end = peek() + readableBytes();
        // 查找 "\r\n"
        const char* p = start;
        while (p < end - 1) {
            if (*p == '\r' && *(p + 1) == '\n') return p;
            ++p;
        }
        return nullptr;
    }

    const char* findChar(char ch) const {
        const char* start = peek();
        const char* end = start + readableBytes();
        return static_cast<const char*>(memchr(start, ch, end - start));
    }

private:
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;

    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    void moveDataToFront() {
        if (readIndex_ > kCheapPrepend) {
            size_t readable = readableBytes();
            std::copy(begin() + readIndex_, begin() + writeIndex_, begin() + kCheapPrepend);
            readIndex_ = kCheapPrepend;
            writeIndex_ = readIndex_ + readable;
        } else {
            // 如果前面空间不足，resize 已经在 ensure 里处理
            // 这里兜底：直接移动（理论上不会触发）
            size_t readable = readableBytes();
            std::copy(begin() + readIndex_, begin() + writeIndex_, begin() + kCheapPrepend);
            readIndex_ = kCheapPrepend;
            writeIndex_ = readIndex_ + readable;
        }
    }
};

//client
int main() {
    Socket client;
    if (!client.createTCP()) {
        std::cout << "client.createTCP() error" << std::endl;
        return -1;
    }
    if (!client.connect("127.0.0.1", 8888)) {
        std::cout << "client.connect() error" << std::endl;
        return -1;
    }
    std::cout << "connect to the server!!!" << std::endl;

    Buffer buf;
    int savedErrno = 0;
    while (1) {
        // 1. 确保 Buffer 是干净的
        buf.retrieveAll();

        // 2. 追加要发送的内容并发送
        buf.append("I am client");
        buf.writeFd(client.fd(), &savedErrno);

        // 3. 读取服务端回复
        ssize_t n = buf.readFd(client.fd(), &savedErrno);
        if (n > 0) {
            std::cout << "recieve message: " << buf.retrieveAllAsString() << std::endl;
        } else if (n == 0) {
            std::cout << "server disconnected gracefully." << std::endl;
            break;
        } else {
            std::cout << "read error, errno: " << savedErrno << std::endl;
            break;
        }

        sleep(1);
    }
    return 0;
}

//server

int main() {
    Socket server;
    // 1. 先创建 TCP socket
    if (!server.createTCP()) {
        std::cout << "server.createTCP() error" << std::endl;
        return -1;
    }
    // 2. 创建后再设置地址复用
    server.set_reuseaddr();

    if (!server.bind(8888, "0.0.0.0")) {
        std::cout << "server.bind() error" << std::endl;
        return -1;
    }
    if (!server.listen()) {
        std::cout << "server.listen() error" << std::endl;
        return -1;
    }
    std::cout << "server waiting for connection..." << std::endl;

    Socket client = server.accept();
    // 3. 校验 accept 拿到的 client 对象，而不是 server 自身
    if (!client.valid()) {
        std::cout << "client socket invalid error" << std::endl;
        return -1;
    }
    std::cout << "client connected!!!" << std::endl;

    Buffer buf;
    int savedErrno = 0;
    while (1) {
        ssize_t n = buf.readFd(client.fd(), &savedErrno);
        if (n > 0) {
            std::cout << "recieve message: " << buf.retrieveAllAsString() << std::endl;
            
            // 发送给客户端
            buf.append("I am server");
            buf.writeFd(client.fd(), &savedErrno);
            // 发送完清理掉 Buffer，防止影响下一次接收
        } else if (n == 0) {
            std::cout << "client disconnected gracefully." << std::endl;
            break;
        } else {
            std::cout << "read error, errno: " << savedErrno << std::endl;
            break;
        }
    }
    return 0;
}