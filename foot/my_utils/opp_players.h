#ifndef OPP_PLAYERS_H
#define OPP_PLAYERS_H

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <deque>
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "../utils/maths.h"
#include "ball_tools.h"

// 常量定义
#define OPP_HISTORY_SIZE 10          // 对手历史数据记录大小
#define OPP_BALL_CONTROL_THRESHOLD 50.0  // 对手球控制阈值(mm)
#define MAX_THREAT_LEVEL 10.0        // 最大威胁等级

/**
 * @brief 对手球员信息与行为管理类
 */
class OppPlayer {
public:
    // 直接访问的基本属性
    int id;                         // 对手ID
    point2f position;               // 当前位置
    point2f velocity;               // 当前速度向量
    double speed;                   // 速度大小
    double orientation;             // 当前朝向(弧度)
    bool isActive;                  // 是否激活/存在
    bool hasBall;                   // 是否持球
    double threatLevel;             // 威胁等级 (0-10)
    
    // 历史数据
    std::deque<point2f> positionHistory;  // 位置历史
    point2f lastPosition;                 // 上一帧位置
    
    /**
     * @brief 构造函数
     * @param opponentId 对手ID
     */
    OppPlayer(int opponentId = -1) : 
        id(opponentId),
        position(0, 0),
        velocity(0, 0),
        speed(0),
        orientation(0),
        isActive(false),
        hasBall(false),
        threatLevel(0),
        lastPosition(0, 0) {
        
        // 初始化历史数据队列
        for (int i = 0; i < OPP_HISTORY_SIZE; i++) {
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
     * @brief 预测对手在指定时间后的位置
     * @param time 预测时间（秒）
     * @return 预测位置
     */
    point2f predictPosition(double time) const {
        return position + velocity * time;
    }
    
    /**
     * @brief 判断对手是否在我方半场
     * @return 是否在我方半场
     */
    bool isInOurHalf() const {
        return position.x < 0;
    }
    
    /**
     * @brief 判断对手是否在对方半场
     * @return 是否在对方半场
     */
    bool isInOpponentHalf() const {
        return position.x > 0;
    }
    
    /**
     * @brief 判断是否在我方禁区内
     * @return 是否在我方禁区
     */
    bool isInOurPenaltyArea() const {
        // 假设禁区范围为矩形，根据实际情况调整坐标
        return position.x < -FIELD_LENGTH_H + 1000 && fabs(position.y) < 1000;
    }
    
    /**
     * @brief 判断是否在对方禁区内
     * @return 是否在对方禁区
     */
    bool isInOpponentPenaltyArea() const {
        // 假设禁区范围为矩形，根据实际情况调整坐标
        return position.x > FIELD_LENGTH_H - 1000 && fabs(position.y) < 1000;
    }
};

/**
 * @brief 对手管理类，处理所有对手的信息和行为
 */
class OppPlayers {
public:
    // 直接访问的成员变量
    std::vector<OppPlayer> players;   // 所有对手信息
    int opponentCount;                // 对手数量
    
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     * @param ballTools 球工具类引用
     */
    OppPlayers(const WorldModel* model, BallTools& ballTools) : 
        model(model),
        ball(ballTools),
        opponentCount(0) {
        
        updateState();
        last_update_time = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief 更新所有对手状态
     * 每周期调用此函数更新信息
     */
    void updateState() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
        double dt = duration / 1000.0; // 转换为秒
        
        // 获取所有对手存在信息
        const bool* exists = model->get_opp_exist_id();
        int goalieId = model->get_opp_goalie();
        
        // 清空并重新填充对手信息
        players.clear();
        opponentCount = 0;
        
        // 遍历所有可能的对手
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            // 排除不存在的球员和守门员(如果需要排除)
            if (exists[i] /*&& i != goalieId*/) {
                OppPlayer opponent(i);
                opponent.isActive = true;
                
                // 更新基本信息
                opponent.position = model->get_opp_player_pos(i);
                opponent.velocity = model->get_opp_player_v(i);
                opponent.orientation = model->get_opp_player_dir(i);
                opponent.speed = opponent.velocity.length();
                
                // 保存上一帧数据（如果有）
                if (opponentCount < oldOpponents.size() && oldOpponents[opponentCount].id == i) {
                    opponent.lastPosition = oldOpponents[opponentCount].position;
                } else {
                    opponent.lastPosition = opponent.position;
                }
                
                // 更新历史数据
                opponent.updateHistory();
                
                // 检查是否持球
                double dist = (ball.position - opponent.position).length();
                double angle_diff = fabs(anglemod((ball.position - opponent.position).angle() - opponent.orientation));
                opponent.hasBall = (dist < OPP_BALL_CONTROL_THRESHOLD && angle_diff < M_PI/4);
                
                // 计算威胁等级
                updateThreatLevel(opponent);
                
                // 添加到对手列表
                players.push_back(opponent);
                opponentCount++;
            }
        }
        
        // 保存当前对手信息用于下一次更新
        oldOpponents = players;
        
        // 更新时间
        last_update_time = current_time;
    }
    
    /**
     * @brief 获取指定ID的对手
     * @param id 对手ID
     * @return 对手对象的引用，如果找不到则返回默认对象
     */
    const OppPlayer& getOpponent(int id) const {
        for (const auto& opponent : players) {
            if (opponent.id == id) {
                return opponent;
            }
        }
        return defaultOpponent;
    }
    
    /**
     * @brief 获取最接近球的对手
     * @return 最接近球的对手，如果没有则返回默认对象
     */
    const OppPlayer& getClosestToBall() const {
        if (players.empty()) {
            return defaultOpponent;
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
     * @brief 获取最接近指定位置的对手
     * @param pos 目标位置
     * @return 最接近的对手，如果没有则返回默认对象
     */
    const OppPlayer& getClosestToPosition(const point2f& pos) const {
        if (players.empty()) {
            return defaultOpponent;
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
     * @brief 获取最接近球的距离
     * @return 最小距离，如果没有对手则返回MAX_DISTANCE
     */
    double getClosestDistanceToBall() const {
        if (players.empty()) {
            return 9999.0; // 最大距离
        }
        
        double minDist = 9999.0;
        
        for (const auto& opponent : players) {
            double dist = opponent.distanceTo(ball.position);
            if (dist < minDist) {
                minDist = dist;
            }
        }
        
        return minDist;
    }
    
    /**
     * @brief 获取持球对手
     * @return 持球对手，如果没有则返回默认对象
     */
    const OppPlayer& getBallHolder() const {
        for (const auto& opponent : players) {
            if (opponent.hasBall) {
                return opponent;
            }
        }
        return defaultOpponent;
    }
    
    /**
     * @brief 判断对手是否持有球
     * @return 是否有对手持球
     */
    bool hasOpponentBall() const {
        for (const auto& opponent : players) {
            if (opponent.hasBall) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief 判断是否有任何对手存在
     * @return 是否有对手
     */
    bool hasOpponent() const {
        return !players.empty();
    }
    
    /**
     * @brief 计算对手的平均位置
     * @return 平均位置
     */
    point2f getAveragePosition() const {
        if (players.empty()) {
            return point2f(0, 0);
        }
        
        point2f sum(0, 0);
        for (const auto& opponent : players) {
            sum = sum + opponent.position;
        }
        
        return sum / static_cast<float>(players.size());
    }
    
    /**
     * @brief 获取威胁最大的对手
     * @return 威胁最大的对手，如果没有则返回默认对象
     */
    const OppPlayer& getMostThreatening() const {
        if (players.empty()) {
            return defaultOpponent;
        }
        
        double maxThreat = -1.0;
        int threatIndex = 0;
        
        for (size_t i = 0; i < players.size(); i++) {
            if (players[i].threatLevel > maxThreat) {
                maxThreat = players[i].threatLevel;
                threatIndex = i;
            }
        }
        
        return players[threatIndex];
    }
    
    /**
     * @brief 获取具有较大威胁的对手数量
     * @param threshold 威胁阈值，默认为5.0
     * @return 威胁对手数量
     */
    int getThreateningCount(double threshold = 5.0) const {
        int count = 0;
        
        for (const auto& opponent : players) {
            if (opponent.threatLevel >= threshold) {
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * @brief 获取在我方半场的对手数量
     * @return 在我方半场的对手数量
     */
    int getCountInOurHalf() const {
        int count = 0;
        
        for (const auto& opponent : players) {
            if (opponent.isInOurHalf()) {
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * @brief 获取在对方半场的对手数量
     * @return 在对方半场的对手数量
     */
    int getCountInOpponentHalf() const {
        int count = 0;
        
        for (const auto& opponent : players) {
            if (opponent.isInOpponentHalf()) {
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * @brief 判断是否有对手在我方禁区
     * @return 是否有对手在我方禁区
     */
    bool isOpponentInOurPenaltyArea() const {
        for (const auto& opponent : players) {
            if (opponent.isInOurPenaltyArea()) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief 预测指定对手在特定时间后的位置
     * @param id 对手ID
     * @param time 预测时间（秒）
     * @return 预测位置
     */
    point2f predictOpponentPosition(int id, double time) const {
        const OppPlayer& opponent = getOpponent(id);
        
        if (opponent.id == -1) {
            return point2f(0, 0); // 对手不存在
        }
        
        return opponent.predictPosition(time);
    }
    
    /**
     * @brief 计算对手前进路径是否会拦截球
     * @param id 对手ID
     * @param lookaheadTime 前瞻时间（秒）
     * @return 是否有拦截可能
     */
    bool willOpponentInterceptBall(int id, double lookaheadTime = 1.0) const {
        const OppPlayer& opponent = getOpponent(id);
        
        if (opponent.id == -1 || ball.velocity.length() < 50) {
            return false; // 对手不存在或球几乎静止
        }
        
        // 预测球和对手未来位置
        point2f futureBallPos = ball.predictPosition(lookaheadTime);
        point2f futureOpponentPos = opponent.predictPosition(lookaheadTime);
        
        // 如果未来位置足够接近，认为可能会拦截
        double interceptDistance = (futureBallPos - futureOpponentPos).length();
        return interceptDistance < 100; // 拦截距离阈值
    }

private:
    const WorldModel* model;
    BallTools& ball;
    std::vector<OppPlayer> oldOpponents;  // 上一帧的对手信息
    OppPlayer defaultOpponent;            // 默认对手对象，用于找不到时返回
    
    // 上次更新时间
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update_time;
    
    /**
     * @brief 更新对手的威胁等级
     * @param opponent 待更新的对手对象
     */
    void updateThreatLevel(OppPlayer& opponent) {
        // 基础威胁值
        double threatLevel = 0.0;
        
        // 位置威胁：越靠近我方球门越危险
        // 我方球门位置
        point2f ourGoal(-FIELD_LENGTH_H, 0);
        double distToGoal = (opponent.position - ourGoal).length();
        double maxDistToGoal = 2 * FIELD_LENGTH_H; // 场地对角线长度
        
        // 距离球门的位置威胁（最多4分）
        threatLevel += 4.0 * (1.0 - std::min(distToGoal / maxDistToGoal, 1.0));
        
        // 速度威胁：向我方球门移动越快越危险
        point2f toGoal = ourGoal - opponent.position;
        double velocityToGoal = opponent.velocity.dot(toGoal) / toGoal.length();
        
        // 朝我方球门移动的速度威胁（最多2分）
        threatLevel += 2.0 * std::max(velocityToGoal / 500.0, 0.0); // 假设最大速度为500单位
        
        // 球权威胁：持球增加威胁（增加3分）
        if (opponent.hasBall) {
            threatLevel += 3.0;
        }
        
        // 靠近球的威胁（最多1分）
        double distToBall = opponent.distanceTo(ball.position);
        if (distToBall < 500) {
            threatLevel += 1.0 * (1.0 - distToBall / 500.0);
        }
        
        // 限制威胁值范围
        opponent.threatLevel = std::min(std::max(threatLevel, 0.0), MAX_THREAT_LEVEL);
    }
};

#endif // OPP_PLAYERS_H