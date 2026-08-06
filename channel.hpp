#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>

template<typename T>
class Channel{
public:
    explicit Channel(size_t capacity=1):capacity_(capacity){
        if (capacity_ == 0) 
            throw std::invalid_argument("Capacity must be > 0 for this simple version");
    }
    //blocking send
    bool send(T value){
        std::unique_lock<std::mutex> lock(mtx_);
        not_full_.wait(lock,[this](){
            return closed_ || buffer_.size() < capacity_;
        });
        if (closed_|| buffer_.size() >= capacity_) return false;
        buffer_.push(std::move(value));
        not_empty_.notify_one();
        return true;    
    }
    //blocking receive
    std::optional<T> receive() {
        std::unique_lock<std::mutex> lock(mtx_);
        not_empty_.wait(lock, [this]() {
            return closed_ || !buffer_.empty();
        });
        if (closed_ && buffer_.empty()) return std::nullopt;
        T value = std::move(buffer_.front());
        buffer_.pop();
        not_full_.notify_one();
        return value;
    }
    //non-blocking send
    bool try_send(T value){
        std::lock_guard<std::mutex> lock(mtx_);
        if (closed_ || buffer_.size() >= capacity_) return false;
        buffer_.push(std::move(value));
        not_empty_.notify_one();
        return true;
    }
    //non-blocking receive
    std::optional<T> try_receive(){
        std::unique_lock<std::mutex>lock(mtx_);
        if (buffer_.empty()) return std::nullopt;
        T value = std::move(buffer_.front());
        buffer_.pop();
        not_full_.notify_one();
        return value;
    }

    void close(){
        std::unique_lock<std::mutex>lock(mtx_);
        closed_=true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

private:
    std::queue<T> buffer_;
    size_t capacity_;
    bool closed_ = false;
    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

