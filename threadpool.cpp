#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
private:
    std::vector<std::thread> workers;        // 工作线程
    std::queue<std::function<void()>> tasks; // 任务队列
    
    std::mutex queue_mutex;                  // 队列锁
    std::condition_variable cv;             // 条件变量
    bool stop = false;                       // 停止标志

public:
    // 初始化
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        // 加锁去取任务
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        // 当队列为空且没有停止时，阻塞等待
                        this->cv.wait(lock, [this] { 
                            return this->stop || !this->tasks.empty(); 
                        });
                        
                        // 如果停止了且任务处理完了，退出线程
                        if (this->stop && this->tasks.empty()) return;
                        
                        // 取得任务
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    // 执行任务
                    task();
                }
            });
        }
    }

    // 提交任务
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) throw std::runtime_error("线程池已停止，无法提交任务");
            tasks.push(task);
        }
        cv.notify_one(); // 唤醒一个线程
    }

    // 析构函数：优雅关闭
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all(); // 唤醒所有线程准备退出
        for (std::thread &worker : workers) {
            if (worker.joinable()) worker.join(); // 等待所有线程结束
        }
    }
};

// --- 测试代码 ---
int main() {
    ThreadPool pool(10); // 4个线程

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            std::cout << "任务 " << i << " 正在被线程 " 
                      << std::this_thread::get_id() << " 执行\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}