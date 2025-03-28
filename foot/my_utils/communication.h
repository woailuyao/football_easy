#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <map>
#include <windows.h>
#include "../utils/vector.h"
#include "../utils/constants.h"
#include "logger.h"

/**
 * @brief 通信消息类型枚举
 */
enum class MessageType {
    NONE,               // 没有消息
    BALL_POSSESSION,    // 球权信息
    PASS_INTENTION,     // 传球意图
    PASS_EXECUTION,     // 执行传球
    POSITION_EXCHANGE,  // 位置交换
    ATTACK_STRATEGY,    // 进攻策略
    DEFENSE_STRATEGY,   // 防守策略
    EMERGENCY           // 紧急情况
};

/**
 * @brief 通信消息结构
 */
struct Message {
    int sender_id;             // 发送者ID
    int receiver_id;           // 接收者ID
    MessageType type;          // 消息类型
    int timestamp;             // 时间戳（周期计数）
    point2f position;          // 相关位置（如传球目标位置）
    double orientation;        // 相关方向
    std::string data;          // 额外数据（字符串格式）

    Message() : sender_id(-1), receiver_id(-1), type(MessageType::NONE), 
                timestamp(0), position(0, 0), orientation(0), data("") {}
};

/**
 * @brief 球员间通信工具类，使用共享内存方式实现
 */
class Communication {
public:
    /**
     * @brief 获取单例实例
     * @return Communication单例
     */
    static Communication& getInstance() {
        static Communication instance;
        return instance;
    }

    /**
     * @brief 初始化通信系统
     * @param robot_id 当前机器人ID
     * @return 是否初始化成功
     */
    bool initialize(int robot_id) {
        this->robot_id = robot_id;
        
        // 尝试创建共享内存
        try {
            // 使用文件映射实现共享内存
            // 实际项目中可能需要考虑更安全的方式，这里使用简化版本
            std::string mapping_name = "Soccer_Robot_Communication";
            
            // 创建或打开文件映射
            h_mapping = CreateFileMapping(
                INVALID_HANDLE_VALUE,    // 使用分页文件
                NULL,                   // 默认安全属性
                PAGE_READWRITE,         // 读写权限
                0,                      // 高位大小
                sizeof(SharedMemory),   // 低位大小
                mapping_name.c_str());  // 映射名称
            
            if (h_mapping == NULL) {
                LOG_ERROR("Failed to create file mapping", robot_id);
                return false;
            }
            
            // 映射视图
            shared_memory = (SharedMemory*)MapViewOfFile(
                h_mapping,              // 文件映射对象
                FILE_MAP_ALL_ACCESS,    // 读写权限
                0,                      // 高位偏移
                0,                      // 低位偏移
                sizeof(SharedMemory));  // 映射大小
            
            if (shared_memory == NULL) {
                LOG_ERROR("Failed to map view of file", robot_id);
                CloseHandle(h_mapping);
                h_mapping = NULL;
                return false;
            }
            
            // 创建互斥锁
            h_mutex = CreateMutex(NULL, FALSE, "Soccer_Robot_Communication_Mutex");
            if (h_mutex == NULL) {
                LOG_ERROR("Failed to create mutex", robot_id);
                UnmapViewOfFile(shared_memory);
                CloseHandle(h_mapping);
                h_mapping = NULL;
                shared_memory = NULL;
                return false;
            }
            
            // 初始化共享内存
            if (WaitForSingleObject(h_mutex, 1000) == WAIT_OBJECT_0) {
                memset(shared_memory, 0, sizeof(SharedMemory));
                ReleaseMutex(h_mutex);
            }
            
            is_initialized = true;
            LOG_INFO("Communication system initialized", robot_id);
            return true;
            
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Communication initialization error: ") + e.what(), robot_id);
            return false;
        }
    }

    /**
     * @brief 发送消息
     * @param receiver_id 接收者ID
     * @param type 消息类型
     * @param position 相关位置
     * @param orientation 相关方向
     * @param data 额外数据
     * @return 是否发送成功
     */
    bool sendMessage(int receiver_id, MessageType type, const point2f& position = point2f(0, 0), 
                   double orientation = 0, const std::string& data = "") {
        if (!is_initialized || shared_memory == NULL) {
            LOG_ERROR("Cannot send message: Communication not initialized", robot_id);
            return false;
        }
        
        // 寻找一个空闲的消息槽
        if (WaitForSingleObject(h_mutex, 1000) == WAIT_OBJECT_0) {
            for (int i = 0; i < MAX_MESSAGES; i++) {
                if (shared_memory->messages[i].type == MessageType::NONE ||
                    shared_memory->messages[i].timestamp < current_cycle - 10) {  // 10周期前的消息可以覆盖
                    
                    // 填充消息
                    shared_memory->messages[i].sender_id = robot_id;
                    shared_memory->messages[i].receiver_id = receiver_id;
                    shared_memory->messages[i].type = type;
                    shared_memory->messages[i].timestamp = current_cycle;
                    shared_memory->messages[i].position = position;
                    shared_memory->messages[i].orientation = orientation;
                    
                    // 截断过长的数据
                    std::string truncated_data = data.substr(0, MAX_DATA_LENGTH - 1);
                    strncpy(shared_memory->messages[i].data, truncated_data.c_str(), MAX_DATA_LENGTH - 1);
                    shared_memory->messages[i].data[MAX_DATA_LENGTH - 1] = '\0';  // 确保字符串结束
                    
                    ReleaseMutex(h_mutex);
                    
                    LOG_DEBUG("Message sent to robot " + std::to_string(receiver_id) + 
                             ", type: " + std::to_string(static_cast<int>(type)), robot_id);
                    return true;
                }
            }
            
            ReleaseMutex(h_mutex);
            LOG_WARNING("Message buffer full", robot_id);
        } else {
            LOG_ERROR("Failed to acquire mutex for sending", robot_id);
        }
        
        return false;
    }

    /**
     * @brief 接收指定类型的最新消息
     * @param type 消息类型，默认为NONE表示接收任何类型
     * @return 接收到的消息，如果没有则返回默认消息（type=NONE）
     */
    Message receiveMessage(MessageType type = MessageType::NONE) {
        Message result;
        if (!is_initialized || shared_memory == NULL) {
            LOG_ERROR("Cannot receive message: Communication not initialized", robot_id);
            return result;
        }
        
        int latest_timestamp = -1;
        
        if (WaitForSingleObject(h_mutex, 1000) == WAIT_OBJECT_0) {
            for (int i = 0; i < MAX_MESSAGES; i++) {
                // 检查消息是否是发给当前机器人的，且是否是指定类型或任意类型
                if (shared_memory->messages[i].receiver_id == robot_id &&
                    (type == MessageType::NONE || shared_memory->messages[i].type == type) &&
                    shared_memory->messages[i].timestamp > latest_timestamp) {
                    
                    // 找到更新的消息
                    latest_timestamp = shared_memory->messages[i].timestamp;
                    
                    // 复制消息
                    result.sender_id = shared_memory->messages[i].sender_id;
                    result.receiver_id = shared_memory->messages[i].receiver_id;
                    result.type = shared_memory->messages[i].type;
                    result.timestamp = shared_memory->messages[i].timestamp;
                    result.position = shared_memory->messages[i].position;
                    result.orientation = shared_memory->messages[i].orientation;
                    result.data = shared_memory->messages[i].data;
                }
            }
            
            ReleaseMutex(h_mutex);
            
            if (latest_timestamp > 0) {
                LOG_DEBUG("Message received from robot " + std::to_string(result.sender_id) + 
                         ", type: " + std::to_string(static_cast<int>(result.type)), robot_id);
            }
        } else {
            LOG_ERROR("Failed to acquire mutex for receiving", robot_id);
        }
        
        return result;
    }

    /**
     * @brief 设置当前周期
     * @param cycle 周期编号
     */
    void setCycle(int cycle) {
        current_cycle = cycle;
    }

    /**
     * @brief 清理通信资源
     */
    void cleanup() {
        if (is_initialized) {
            if (shared_memory != NULL) {
                UnmapViewOfFile(shared_memory);
                shared_memory = NULL;
            }
            
            if (h_mapping != NULL) {
                CloseHandle(h_mapping);
                h_mapping = NULL;
            }
            
            if (h_mutex != NULL) {
                CloseHandle(h_mutex);
                h_mutex = NULL;
            }
            
            is_initialized = false;
            LOG_INFO("Communication system cleaned up", robot_id);
        }
    }
    
    /**
     * @brief 广播球权信息
     * @param has_ball 是否持球
     * @param ball_pos 球的位置
     * @return 是否广播成功
     */
    bool broadcastBallPossession(bool has_ball, const point2f& ball_pos) {
        // 向所有其他球员发送消息
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (i != robot_id) {
                std::string data = has_ball ? "1" : "0";  // 1表示持球，0表示没有
                if (!sendMessage(i, MessageType::BALL_POSSESSION, ball_pos, 0, data)) {
                    return false;
                }
            }
        }
        return true;
    }
    
    /**
     * @brief 发送传球意图
     * @param receiver_id 接球者ID
     * @param target_pos 传球目标位置
     * @return 是否发送成功
     */
    bool sendPassIntention(int receiver_id, const point2f& target_pos) {
        return sendMessage(receiver_id, MessageType::PASS_INTENTION, target_pos);
    }
    
    /**
     * @brief 发送传球执行消息
     * @param receiver_id 接球者ID
     * @param target_pos 传球目标位置
     * @param pass_power 传球力度
     * @return 是否发送成功
     */
    bool sendPassExecution(int receiver_id, const point2f& target_pos, double pass_power) {
        std::string power_str = std::to_string(pass_power);
        return sendMessage(receiver_id, MessageType::PASS_EXECUTION, target_pos, 0, power_str);
    }
    
    /**
     * @brief 发送位置交换请求
     * @param receiver_id 接收者ID
     * @param my_pos 自己的目标位置
     * @param other_pos 对方的目标位置
     * @return 是否发送成功
     */
    bool sendPositionExchange(int receiver_id, const point2f& my_pos, const point2f& other_pos) {
        std::stringstream ss;
        ss << other_pos.x << "," << other_pos.y;
        return sendMessage(receiver_id, MessageType::POSITION_EXCHANGE, my_pos, 0, ss.str());
    }
    
    /**
     * @brief 发送进攻策略
     * @param receiver_id 接收者ID
     * @param strategy_code 策略代码
     * @return 是否发送成功
     */
    bool sendAttackStrategy(int receiver_id, int strategy_code) {
        return sendMessage(receiver_id, MessageType::ATTACK_STRATEGY, point2f(0, 0), 0, std::to_string(strategy_code));
    }
    
    /**
     * @brief 发送防守策略
     * @param receiver_id 接收者ID
     * @param strategy_code 策略代码
     * @return 是否发送成功
     */
    bool sendDefenseStrategy(int receiver_id, int strategy_code) {
        return sendMessage(receiver_id, MessageType::DEFENSE_STRATEGY, point2f(0, 0), 0, std::to_string(strategy_code));
    }
    
    /**
     * @brief 发送紧急情况消息
     * @param receiver_id 接收者ID
     * @param emergency_code 紧急情况代码
     * @param position 紧急情况相关位置
     * @return 是否发送成功
     */
    bool sendEmergency(int receiver_id, int emergency_code, const point2f& position) {
        return sendMessage(receiver_id, MessageType::EMERGENCY, position, 0, std::to_string(emergency_code));
    }

private:
    // 最大消息数
    static const int MAX_MESSAGES = 20;
    
    // 最大数据长度
    static const int MAX_DATA_LENGTH = 256;
    
    // 共享内存结构
    struct SharedMemoryMessage {
        int sender_id;
        int receiver_id;
        MessageType type;
        int timestamp;
        point2f position;
        double orientation;
        char data[MAX_DATA_LENGTH];
    };
    
    struct SharedMemory {
        SharedMemoryMessage messages[MAX_MESSAGES];
    };
    
    // 构造函数私有化
    Communication() : robot_id(-1), current_cycle(0), is_initialized(false), 
                     shared_memory(NULL), h_mapping(NULL), h_mutex(NULL) {}
    
    // 析构函数
    ~Communication() {
        cleanup();
    }
    
    // 禁用拷贝和赋值
    Communication(const Communication&) = delete;
    Communication& operator=(const Communication&) = delete;
    
    // 成员变量
    int robot_id;
    int current_cycle;
    bool is_initialized;
    SharedMemory* shared_memory;
    HANDLE h_mapping;
    HANDLE h_mutex;
};

#endif // COMMUNICATION_H 