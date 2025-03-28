#ifndef TIMER_H
#define TIMER_H

#include <string>
#include <map>
#include <chrono>

// 计时器类
class Timer {
private:
    static std::map<std::string, std::chrono::steady_clock::time_point> start_times;
    
public:
    // 开始计时
    static void start(const std::string& timer_name) {
        start_times[timer_name] = std::chrono::steady_clock::now();
    }
    
    // 获取已过时间(秒)
    static double elapsed(const std::string& timer_name) {
        if (start_times.find(timer_name) == start_times.end()) {
            return 0.0;
        }
        
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_times[timer_name]).count();
    }
    
    // 检查是否超过指定时间
    static bool has_elapsed(const std::string& timer_name, double seconds) {
        return elapsed(timer_name) >= seconds;
    }
    
    // 重置计时器
    static void reset(const std::string& timer_name) {
        start_times[timer_name] = std::chrono::steady_clock::now();
    }
};

// 静态成员初始化
std::map<std::string, std::chrono::steady_clock::time_point> Timer::start_times;

#endif // TIMER_H 