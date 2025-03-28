#ifndef TEAMMATES_H
#define TEAMMATES_H

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <deque>
#include "../utils/robot.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "../utils/maths.h"
#include "ball_tools.h"

// 常量定义
#define HISTORY_SIZE 10       // 历史数据记录大小
#define BALL_CONTROL_THRESHOLD 50.0  // 球控制阈值(mm)

/**
 * @brief 队友信息与行为管理类
 * 这个类关注队友的状态和行为预测
 */
class Teammate {
public:
    // 直接访问的基本属性
    int id;                         // 队友ID
    point2f position;               // 当前位置
    point2f velocity;               // 当前速度向量
    double speed;                   // 速度大小
    double orientation;             // 当前朝向(弧度)
    bool isActive;                  // 是否激活/存在
    bool hasBall;                   // 是否持球
    
    // 历史数据
    std::deque<point2f> positionHistory;  // 位置历史
    point2f lastPosition;                 // 上一帧位置
    
    /**
     * @brief 构造函数
     * @param teammateId 队友ID
     */
    Teammate(int teammateId = -1) : 
        id(teammateId),
        position(0, 0),
        velocity(0, 0),
        speed(0),
        orientation(0),
        isActive(false),
        hasBall(false),
        lastPosition(0, 0) {
        
        // 初始化历史数据队列
        for (int i = 0; i < HISTORY_SIZE; i++) {
            positionHistory.push_back(point2f(0, 0));
        }
    }
    
    /**
     * @brief 更新历史数据
     */
    void updateHistory() {
        positionHistory.pop_back();
        positionHistory.push_front(position);
    }
    
    /**
     * @brief 获取前进方向的单位向量
     * @return 前进方向向量
     */
    point2f getDirectionVector() const {
        return point2f(cos(orientation), sin(orientation));
    }
    
    /**
     * @brief 计算到指定点的距离
     * @param target 目标点
     * @return 距离
     */
    double distanceTo(const point2f& target) const {
        return (position - target).length();
    }
    
    /**
     * @brief 预测队友在指定时间后的位置
     * @param time 预测时间（秒）
     * @return 预测位置
     */
    point2f predictPosition(double time) const {
        return position + velocity * time;
    }
    
    /**
     * @brief 判断队友是否在我方半场
     * @return 是否在我方半场
     */
    bool isInOurHalf() const {
        return position.x < 0;
    }
    
    /**
     * @brief 判断队友是否在对方半场
     * @return 是否在对方半场
     */
    bool isInOpponentHalf() const {
        return position.x > 0;
    }
};

/**
 * @brief 队友管理类，处理所有队友的信息和行为
 */
class Teammates {
public:
    // 直接访问的成员变量
    std::vector<Teammate> players;   // 所有队友信息
    int selfId;                      // 自身ID（用于排除）
    int teammateCount;               // 队友数量
    
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     * @param ballTools 球工具类引用
     * @param robotId 当前机器人ID
     */
    Teammates(const WorldModel* model, BallTools& ballTools, int robotId) : 
        model(model),
        ball(ballTools),
        selfId(robotId),
        teammateCount(0) {
        
        updateState();
        last_update_time = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief 更新所有队友状态
     * 每周期调用此函数更新信息
     */
    void updateState() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
        double dt = duration / 1000.0; // 转换为秒
        
        // 获取所有球员存在信息
        const bool* exists = model->get_our_exist_id();
        int goalieId = model->get_our_goalie();
        
        // 清空并重新填充队友信息
        players.clear();
        teammateCount = 0;
        
        // 遍历所有可能的队友
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            // 排除自己、守门员和不存在的球员
            if (exists[i] && i != selfId && i != goalieId) {
                Teammate teammate(i);
                teammate.isActive = true;
                
                // 更新基本信息
                teammate.position = model->get_our_player_pos(i);
                teammate.velocity = model->get_our_player_v(i);
                teammate.orientation = model->get_our_player_dir(i);
                teammate.speed = teammate.velocity.length();
                
                // 保存上一帧数据（如果有）
                if (teammateCount < oldTeammates.size() && oldTeammates[teammateCount].id == i) {
                    teammate.lastPosition = oldTeammates[teammateCount].position;
                } else {
                    teammate.lastPosition = teammate.position;
                }
                
                // 更新历史数据
                teammate.updateHistory();
                
                // 检查是否持球
                double dist = (ball.position - teammate.position).length();
                double angle_diff = fabs(anglemod((ball.position - teammate.position).angle() - teammate.orientation));
                teammate.hasBall = (dist < BALL_CONTROL_THRESHOLD && angle_diff < M_PI/4);
                
                // 添加到队友列表
                players.push_back(teammate);
                teammateCount++;
            }
        }
        
        // 保存当前队友信息用于下一次更新
        oldTeammates = players;
        
        // 更新时间
        last_update_time = current_time;
    }
    
    /**
     * @brief 获取指定ID的队友
     * @param id 队友ID
     * @return 队友对象的引用，如果找不到则返回默认对象
     */
    const Teammate& getTeammate(int id) const {
        for (const auto& teammate : players) {
            if (teammate.id == id) {
                return teammate;
            }
        }
        return defaultTeammate;
    }
    
    /**
     * @brief 获取最接近球的队友
     * @return 最接近球的队友，如果没有则返回默认对象
     */
    const Teammate& getClosestToBall() const {
        if (players.empty()) {
            return defaultTeammate;
        }
        
        double minDist = 9999.0;
        int closestIndex = 0;
        
        for (size_t i = 0; i < players.size(); i++) {
            double dist = players[i].distanceTo(ball.position);
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }
        
        return players[closestIndex];
    }
    
    /**
     * @brief 获取最接近指定位置的队友
     * @param pos 目标位置
     * @return 最接近的队友，如果没有则返回默认对象
     */
    const Teammate& getClosestToPosition(const point2f& pos) const {
        if (players.empty()) {
            return defaultTeammate;
        }
        
        double minDist = 9999.0;
        int closestIndex = 0;
        
        for (size_t i = 0; i < players.size(); i++) {
            double dist = players[i].distanceTo(pos);
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }
        
        return players[closestIndex];
    }
    
    /**
     * @brief 获取持球队友
     * @return 持球队友，如果没有则返回默认对象
     */
    const Teammate& getBallHolder() const {
        for (const auto& teammate : players) {
            if (teammate.hasBall) {
                return teammate;
            }
        }
        return defaultTeammate;
    }
    
    /**
     * @brief 判断队友是否持有球
     * @return 是否有队友持球
     */
    bool hasTeammateBall() const {
        for (const auto& teammate : players) {
            if (teammate.hasBall) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief 判断是否有任何队友存在
     * @return 是否有队友
     */
    bool hasTeammate() const {
        return !players.empty();
    }
    
    /**
     * @brief 计算队友的平均位置
     * @return 平均位置
     */
    point2f getAveragePosition() const {
        if (players.empty()) {
            return point2f(0, 0);
        }
        
        point2f sum(0, 0);
        for (const auto& teammate : players) {
            sum = sum + teammate.position;
        }
        
        return sum / static_cast<float>(players.size());
    }
    
    /**
     * @brief 计算队友间的最大距离
     * @return 最大距离，如果不足两名队友则返回0
     */
    double getMaxTeammateDistance() const {
        if (players.size() < 2) {
            return 0.0;
        }
        
        double maxDist = 0.0;
        
        for (size_t i = 0; i < players.size(); i++) {
            for (size_t j = i+1; j < players.size(); j++) {
                double dist = players[i].distanceTo(players[j].position);
                if (dist > maxDist) {
                    maxDist = dist;
                }
            }
        }
        
        return maxDist;
    }
    
    /**
     * @brief 预测最佳传球接收位置
     * @param receiverId 接球队友ID，如果为-1则选择最合适的队友
     * @return 最佳接球位置
     */
    point2f predictBestReceivingPosition(int receiverId = -1) const {
        // 如果没有队友，返回默认位置（前场）
        if (players.empty()) {
            return point2f(FIELD_LENGTH_H / 2, 0);
        }
        
        // 如果未指定接球者，选择最远离球且在前场的队友
        const Teammate* receiver = nullptr;
        
        if (receiverId == -1) {
            double maxDist = -1.0;
            
            for (const auto& teammate : players) {
                // 优先选择在对方半场的队友
                if (teammate.isInOpponentHalf()) {
                    double dist = teammate.distanceTo(ball.position);
                    if (dist > maxDist) {
                        maxDist = dist;
                        receiver = &teammate;
                    }
                }
            }
            
            // 如果没有在对方半场的队友，选择任意最远的队友
            if (!receiver) {
                maxDist = -1.0;
                for (const auto& teammate : players) {
                    double dist = teammate.distanceTo(ball.position);
                    if (dist > maxDist) {
                        maxDist = dist;
                        receiver = &teammate;
                    }
                }
            }
        } else {
            // 使用指定ID的队友
            for (const auto& teammate : players) {
                if (teammate.id == receiverId) {
                    receiver = &teammate;
                    break;
                }
            }
        }
        
        // 如果找不到合适的接球者，返回默认位置
        if (!receiver) {
            return point2f(FIELD_LENGTH_H / 2, 0);
        }
        
        // 计算队友前进的位置
        double advanceTime = 0.5; // 0.5秒后位置
        point2f receiverFuturePos = receiver->position + receiver->velocity * advanceTime;
        
        // 朝向球门的方向
        point2f goalPos(FIELD_LENGTH_H, 0);
        point2f towardsGoal = goalPos - receiverFuturePos;
        
        // 标准化向量
        if (towardsGoal.length() > 0.1) {
            towardsGoal = towardsGoal * (1.0 / towardsGoal.length());
        }
        
        // 预留接球空间
        point2f bestReceivingPoint = receiverFuturePos + towardsGoal * 100; // 前进100单位
        
        return bestReceivingPoint;
    }

private:
    const WorldModel* model;
    BallTools& ball;
    std::vector<Teammate> oldTeammates;  // 上一帧的队友信息
    Teammate defaultTeammate;            // 默认队友对象，用于找不到时返回
    
    // 上次更新时间
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update_time;
};

#endif // TEAMMATES_H 