#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>

// 定义日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    LOG_ERROR, // 避免与 Windows API / 系统宏冲突
    FATAL
};

class Logger {
public:
    // 获取单例实例
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 设置最小打印级别
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentLevel_ = level;
    }

    // 设置日志文件输出（传入空字符串则仅输出到控制台）
    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
        if (!filename.empty()) {
            fileStream_.open(filename, std::ios::out | std::ios::app);
        }
    }

    // 写入日志的主逻辑
    void log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0) {
        if (level < currentLevel_) return;

        std::string timeStr = getCurrentTime();
        std::string levelStr = levelToString(level);

        std::ostringstream logEntry;
        logEntry << "[" << timeStr << "] [" << levelStr << "] ";
        if (file) {
            logEntry << "[" << file << ":" << line << "] ";
        }
        logEntry << message << "\n";

        // 线程安全输出
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << logEntry.str() << std::flush;
        
        if (fileStream_.is_open()) {
            fileStream_ << logEntry.str() << std::flush;
        }
    }

    ~Logger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

private:
    Logger() : currentLevel_(LogLevel::INFO) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 获取格式化时间戳 (YYYY-MM-DD HH:MM:SS.mmm)
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:     return "DEBUG";
            case LogLevel::INFO:      return "INFO";
            case LogLevel::WARNING:   return "WARN";
            case LogLevel::LOG_ERROR: return "ERROR";
            case LogLevel::FATAL:     return "FATAL";
            default:                  return "UNKNOWN";
        }
    }

    LogLevel currentLevel_;
    std::ofstream fileStream_;
    std::mutex mutex_;
};

// 快捷宏定义，自动捕获代码行号与文件名，同时支持流式拼接
#define LOG_DEBUG(msg) { std::ostringstream _ss; _ss << msg; Logger::getInstance().log(LogLevel::DEBUG, _ss.str(), __FILE__, __LINE__); }
#define LOG_INFO(msg)  { std::ostringstream _ss; _ss << msg; Logger::getInstance().log(LogLevel::INFO, _ss.str(), __FILE__, __LINE__); }
#define LOG_WARN(msg)  { std::ostringstream _ss; _ss << msg; Logger::getInstance().log(LogLevel::WARNING, _ss.str(), __FILE__, __LINE__); }
#define LOG_ERROR(msg) { std::ostringstream _ss; _ss << msg; Logger::getInstance().log(LogLevel::LOG_ERROR, _ss.str(), __FILE__, __LINE__); }
#define LOG_FATAL(msg) { std::ostringstream _ss; _ss << msg; Logger::getInstance().log(LogLevel::FATAL, _ss.str(), __FILE__, __LINE__); }