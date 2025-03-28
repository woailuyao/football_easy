#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <windows.h>
#include <vector>
#include <mutex>
#include <map>
#include <cmath>
#include "../utils/vector.h"

// 调试输出函数
inline void debug_output(const std::string& message) {
    OutputDebugStringA((message + "\n").c_str());
}

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG,      // 调试信息
    INFO,       // 普通信息
    WARNING,    // 警告信息
    ERROR_LEVEL,     // 错误信息
    CRITICAL    // 严重错误
};

/**
 * @brief 日志记录工具类，单例模式
 */
class Logger {
public:
    /**
     * @brief 获取单例实例
     * @return Logger单例
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLogLevel(LogLevel level) {
        current_level = level;
    }
    
    /**
     * @brief 启用或禁用文件日志
     * @param enable 是否启用
     * @param filename 日志文件名
     */
    void setFileLogging(bool enable, const std::string& filename = "robot_log.txt") {
        file_logging = enable;
        log_filename = filename;
        
        if (enable && !log_file.is_open()) {
            log_file.open(filename, std::ios::out | std::ios::app);
        } else if (!enable && log_file.is_open()) {
            log_file.close();
        }
    }
    
    /**
     * @brief 启用或禁用调试输出
     * @param enable 是否启用
     */
    void setDebugOutput(bool enable) {
        debug_output_enabled = enable;
    }
    
    /**
     * @brief 记录调试级别日志
     * @param message 日志消息
     * @param robot_id 机器人ID，默认为-1
     */
    void debug(const std::string& message, int robot_id = -1) {
        log(LogLevel::DEBUG, message, robot_id);
    }
    
    /**
     * @brief 记录信息级别日志
     * @param message 日志消息
     * @param robot_id 机器人ID，默认为-1
     */
    void info(const std::string& message, int robot_id = -1) {
        log(LogLevel::INFO, message, robot_id);
    }
    
    /**
     * @brief 记录警告级别日志
     * @param message 日志消息
     * @param robot_id 机器人ID，默认为-1
     */
    void warning(const std::string& message, int robot_id = -1) {
        log(LogLevel::WARNING, message, robot_id);
    }
    
    /**
     * @brief 记录错误级别日志
     * @param message 日志消息
     * @param robot_id 机器人ID，默认为-1
     */
    void error_log(const std::string& message, int robot_id = -1) {
        log(LogLevel::ERROR_LEVEL, message, robot_id);
    }
    
    /**
     * @brief 记录严重错误级别日志
     * @param message 日志消息
     * @param robot_id 机器人ID，默认为-1
     */
    void critical(const std::string& message, int robot_id = -1) {
        log(LogLevel::CRITICAL, message, robot_id);
    }
    
    /**
     * @brief 记录位置信息
     * @param prefix 前缀说明
     * @param pos 位置对象
     * @param robot_id 机器人ID，默认为-1
     */
    void logPosition(const std::string& prefix, const point2f& pos, int robot_id = -1) {
        std::stringstream ss;
        ss << prefix << " - Position: (" << pos.x << ", " << pos.y << ")";
        log(LogLevel::INFO, ss.str(), robot_id);
    }
    
    /**
     * @brief 记录向量信息
     * @param prefix 前缀说明
     * @param vec 向量对象
     * @param robot_id 机器人ID，默认为-1
     */
    void logVector(const std::string& prefix, const point2f& vec, int robot_id = -1) {
        std::stringstream ss;
        ss << prefix << " - Vector: (" << vec.x << ", " << vec.y << "), Magnitude: " << vec.length();
        log(LogLevel::INFO, ss.str(), robot_id);
    }
    
    /**
     * @brief 记录角度信息
     * @param prefix 前缀说明
     * @param angle_rad 角度（弧度）
     * @param robot_id 机器人ID，默认为-1
     */
    void logAngle(const std::string& prefix, double angle_rad, int robot_id = -1) {
        std::stringstream ss;
        ss << prefix << " - Angle: " << angle_rad << " rad (" << angle_rad * 180.0 / M_PI << " deg)";
        log(LogLevel::INFO, ss.str(), robot_id);
    }
    
    /**
     * @brief 记录任务状态
     * @param task_name 任务名称
     * @param status 任务状态
     * @param robot_id 机器人ID，默认为-1
     */
    void logTaskStatus(const std::string& task_name, const std::string& status, int robot_id = -1) {
        std::stringstream ss;
        ss << "Task: " << task_name << " - Status: " << status;
        log(LogLevel::INFO, ss.str(), robot_id);
    }
    
    /**
     * @brief 开始计时
     * @param section_name 计时区段名称
     */
    void startTiming(const std::string& section_name) {
        timing_map[section_name] = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief 结束计时并记录
     * @param section_name 计时区段名称
     * @param robot_id 机器人ID，默认为-1
     */
    void endTiming(const std::string& section_name, int robot_id = -1) {
        if (timing_map.find(section_name) == timing_map.end()) {
            warning("Cannot end timing for section that has not been started: " + section_name, robot_id);
            return;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto start_time = timing_map[section_name];
        
        try {
            // 计算耗时（微秒）
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            
            std::stringstream ss;
            ss << "Timing for [" << section_name << "]: " << duration << " µs (" << (duration / 1000.0) << " ms)";
            log(LogLevel::DEBUG, ss.str(), robot_id);
            
            // 从映射中移除计时点
            timing_map.erase(section_name);
        } catch (const std::exception& e) {
            error_log("Error calculating timing: " + std::string(e.what()), robot_id);
        }
    }
    
    /**
     * @brief 记录周期开始
     * @param cycle_num 周期编号
     * @param robot_id 机器人ID，默认为-1
     */
    void logCycleStart(int cycle_num, int robot_id = -1) {
        std::stringstream ss;
        ss << "===== CYCLE " << cycle_num << " START =====";
        log(LogLevel::INFO, ss.str(), robot_id);
        startTiming("cycle_" + std::to_string(cycle_num));
    }
    
    /**
     * @brief 记录周期结束
     * @param cycle_num 周期编号
     * @param robot_id 机器人ID，默认为-1
     */
    void logCycleEnd(int cycle_num, int robot_id = -1) {
        endTiming("cycle_" + std::to_string(cycle_num), robot_id);
        std::stringstream ss;
        ss << "===== CYCLE " << cycle_num << " END =====";
        log(LogLevel::INFO, ss.str(), robot_id);
    }

private:
    Logger() : current_level(LogLevel::INFO), file_logging(false), debug_output_enabled(true) {}
    ~Logger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }
    
    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param message 日志消息
     * @param robot_id 机器人ID
     */
    void log(LogLevel level, const std::string& message, int robot_id) {
        // 检查日志级别
        if (level < current_level) {
            return;
        }
        
        // 构建日志消息
        std::stringstream log_stream;
        
        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        char time_buf[20];
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&time));
        
        log_stream << time_buf << "." << std::setfill('0') << std::setw(3) << ms.count() << " ";
        
        // 添加机器人ID
        if (robot_id >= 0) {
            log_stream << "[Robot " << robot_id << "] ";
        }
        
        // 添加日志级别
        log_stream << "[";
        switch (level) {
            case LogLevel::DEBUG: log_stream << "DEBUG"; break;
            case LogLevel::INFO: log_stream << "INFO"; break;
            case LogLevel::WARNING: log_stream << "WARNING"; break;
            case LogLevel::ERROR_LEVEL: log_stream << "ERROR"; break;
            case LogLevel::CRITICAL: log_stream << "CRITICAL"; break;
        }
        log_stream << "] ";
        
        // 添加消息
        log_stream << message;
        
        // 输出到调试窗口
        if (debug_output_enabled) {
            debug_output(log_stream.str());
        }
        
        // 输出到文件
        if (file_logging && log_file.is_open()) {
            log_file << log_stream.str() << std::endl;
        }
    }

    LogLevel current_level;
    bool file_logging;
    bool debug_output_enabled;
    std::string log_filename;
    std::ofstream log_file;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> timing_map;
};

// 方便使用的宏定义
#define LOG_DEBUG(msg, id) Logger::getInstance().debug(msg, id)
#define LOG_INFO(msg, id) Logger::getInstance().info(msg, id)
#define LOG_WARNING(msg, id) Logger::getInstance().warning(msg, id)
#define LOG_ERROR(msg, id) Logger::getInstance().error_log(msg, id)
#define LOG_CRITICAL(msg, id) Logger::getInstance().critical(msg, id)
#define LOG_POSITION(prefix, pos, id) Logger::getInstance().logPosition(prefix, pos, id)
#define LOG_VECTOR(prefix, vec, id) Logger::getInstance().logVector(prefix, vec, id)
#define LOG_ANGLE(prefix, angle, id) Logger::getInstance().logAngle(prefix, angle, id)
#define LOG_TASK(task, status, id) Logger::getInstance().logTaskStatus(task, status, id)
#define LOG_TIMING_START(section) Logger::getInstance().startTiming(section)
#define LOG_TIMING_END(section, id) Logger::getInstance().endTiming(section, id)
#define LOG_CYCLE_START(cycle, id) Logger::getInstance().logCycleStart(cycle, id)
#define LOG_CYCLE_END(cycle, id) Logger::getInstance().logCycleEnd(cycle, id)

#endif // LOGGER_H 