#ifndef PLAYERS_H
#define PLAYERS_H

#include <iostream>
#include <vector>
#include <cmath>
#include <deque>
#include <chrono>
#include "../utils/vector.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/maths.h"
#include "../utils/PlayerTask.h"
#include "ball_tools.h"

// 常量定义
#define PLAYER_HISTORY_SIZE 20     // 历史数据记录大小
#define PLAYER_BALL_CONTROL_DIST 50.0  // 球控制距离阈值

/**
 * @brief 球员类，管理自身机器人的状态和行为
 */
class Player {
public:
    // 可直接访问的基本属性
    int id;                          // 机器人ID
    point2f position;                // 当前位置
    point2f velocity;                // 当前速度向量
    double speed;                    // 速度大小
    double orientation;              // 当前朝向(弧度)
    double rotSpeed;                 // 旋转速度
    bool hasBall;                    // 是否持有球
    bool isActive;                   // 是否处于激活状态
    
    // 玩家能力属性
    double maxSpeed;                 // 最大速度
    double maxAcceleration;          // 最大加速度
    double maxRotSpeed;              // 最大旋转速度
    double kickPower;                // 踢球力量
    double stamina;                  // 体力值(0-100)
    
    // 历史数据
    std::deque<point2f> positionHistory;  // 位置历史
    std::deque<point2f> velocityHistory;  // 速度历史
    std::deque<double> orientationHistory; // 朝向历史
    
    /**
     * @brief 构造函数
     * @param playerId 球员ID
     * @param worldModel 世界模型指针
     * @param ballTools 球工具类引用
     */
    Player(int playerId, const WorldModel* worldModel, BallTools& ballTools) :
        id(playerId),
        position(0, 0),
        velocity(0, 0),
        speed(0),
        orientation(0),
        rotSpeed(0),
        hasBall(false),
        isActive(false),
        maxSpeed(500),
        maxAcceleration(300),
        maxRotSpeed(M_PI),
        kickPower(100),
        stamina(100),
        model(worldModel),
        ball(ballTools) {
        
        // 初始化历史数据队列
        for (int i = 0; i < PLAYER_HISTORY_SIZE; i++) {
            positionHistory.push_back(point2f(0, 0));
            velocityHistory.push_back(point2f(0, 0));
            orientationHistory.push_back(0);
        }
        
        // 初始化更新时间
        last_update_time = std::chrono::high_resolution_clock::now();
        
        // 初始状态更新
        updateState();
    }
    
    /**
     * @brief 更新球员状态
     * 每周期调用此函数更新玩家状态
     */
    void updateState() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
        double dt = duration / 1000.0; // 转换为秒
        
        // 更新球员基本信息
        isActive = model->get_our_exist_id()[id];
        if (!isActive) {
            return; // 如果球员不存在则不更新
        }
        
        // 保存上一帧的位置和朝向用于计算速度
        point2f prevPosition = position;
        double prevOrientation = orientation;
        
        // 获取当前位置和朝向
        position = model->get_our_player_pos(id);
        orientation = model->get_our_player_dir(id);
        
        // 获取当前速度向量（如果可能使用更平滑的估计）
        if (dt > 0.0001) {
            // 这里假设使用model的速度更准确，如果不准，可以使用下面的计算方法
            velocity = model->get_our_player_v(id);
            
            // 或者使用位置差异计算
            // velocity = (position - prevPosition) / dt;
            
            // 计算旋转速度
            rotSpeed = anglemod(orientation - prevOrientation) / dt;
        }
        
        // 计算速度大小
        speed = velocity.length();
        
        // 更新是否持球状态
        updateBallPossession();
        
        // 更新历史数据
        updateHistory();
        
        // 更新体力
        updateStamina(dt);
        
        // 更新时间
        last_update_time = current_time;
    }
    
    /**
     * @brief 更新历史数据
     */
    void updateHistory() {
        // 更新位置历史
        positionHistory.pop_back();
        positionHistory.push_front(position);
        
        // 更新速度历史
        velocityHistory.pop_back();
        velocityHistory.push_front(velocity);
        
        // 更新朝向历史
        orientationHistory.pop_back();
        orientationHistory.push_front(orientation);
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
     * @brief 计算朝向与目标方向的夹角
     * @param target 目标点
     * @return 夹角（弧度）
     */
    double angleTo(const point2f& target) const {
        point2f toTarget = target - position;
        double targetAngle = atan2(toTarget.y, toTarget.x);
        return anglemod(targetAngle - orientation);
    }
    
    /**
     * @brief 预测在指定时间后的位置
     * @param time 预测时间（秒）
     * @return 预测位置
     */
    point2f predictPosition(double time) const {
        return position + velocity * time;
    }
    
    /**
     * @brief 预测移动到目标位置所需的时间
     * @param target 目标位置
     * @return 所需时间（秒）
     */
    double timeToReachPosition(const point2f& target) const {
        double dist = distanceTo(target);
        
        // 简化模型：假设加速到最大速度的一半距离
        double accelDist = 0.5 * maxSpeed * maxSpeed / maxAcceleration;
        
        if (dist <= 2 * accelDist) {
            // 短距离：加速再减速
            return 2 * sqrt(dist / maxAcceleration);
        } else {
            // 长距离：加速、恒速、减速
            double accelTime = maxSpeed / maxAcceleration;
            double constSpeedDist = dist - 2 * accelDist;
            double constSpeedTime = constSpeedDist / maxSpeed;
            return 2 * accelTime + constSpeedTime;
        }
    }
    
    /**
     * @brief 判断是否能在指定时间内到达目标位置
     * @param target 目标位置
     * @param maxTime 最大允许时间
     * @return 是否能到达
     */
    bool canReachInTime(const point2f& target, double maxTime) const {
        return timeToReachPosition(target) <= maxTime;
    }
    
    /**
     * @brief 获取前进方向的单位向量
     * @return 前进方向向量
     */
    point2f getDirectionVector() const {
        return point2f(cos(orientation), sin(orientation));
    }
    
    /**
     * @brief 判断球员是否在我方半场
     * @return 是否在我方半场
     */
    bool isInOurHalf() const {
        return position.x < 0;
    }
    
    /**
     * @brief 判断球员是否在对方半场
     * @return 是否在对方半场
     */
    bool isInOpponentHalf() const {
        return position.x > 0;
    }
    
    /**
     * @brief 计算射门概率
     * 基于当前位置、朝向、和对方守门员位置
     * @return 射门成功概率 (0-1)
     */
    double calculateShotProbability() const {
        // 目标球门位置（假设是对方球门中心）
        point2f goalCenter(FIELD_LENGTH_H, 0);
        double distToGoal = distanceTo(goalCenter);
        
        // 射门初始概率基于距离递减
        double baseProbability = 1.0 - std::min(distToGoal / (FIELD_LENGTH_H * 1.5), 1.0);
        
        // 考虑朝向因素：朝向与射门方向的夹角
        double angleToGoal = fabs(angleTo(goalCenter));
        double angleFactor = 1.0 - std::min(angleToGoal / M_PI, 1.0);
        
        // 考虑对方守门员位置（需要额外信息）
        double goalieFactor = 0.8; // 简化模型
        
        // 结合所有因素
        double probability = baseProbability * angleFactor * goalieFactor;
        
        return std::max(std::min(probability, 1.0), 0.0);
    }
    
    /**
     * @brief 创建移动到指定位置的任务
     * @param targetPos 目标位置
     * @param finalOrientation 最终朝向，默认为当前朝向
     * @param needBall 是否需要带球
     * @return 创建的任务
     */
    PlayerTask createMoveTask(const point2f& targetPos, double finalOrientation = NAN, bool needBall = false) const {
        CPlayerTask task;
        
        // 设置任务类型和目标位置
        TaskT t = needBall ? task.makeTask(PlayerRole::GOALIE, TaskT::Dribble) : 
                             task.makeTask(PlayerRole::GOALIE, TaskT::SmartGoto);
        t.player.pos = targetPos;
        
        // 设置朝向（如果指定）
        if (!std::isnan(finalOrientation)) {
            t.player.angle = finalOrientation;
        }
        
        return t;
    }
    
    /**
     * @brief 创建射门任务
     * @param power 射门力量（0-100）
     * @param target 目标位置，默认为球门中心
     * @return 创建的任务
     */
    PlayerTask createShootTask(double power = 100, point2f target = point2f(FIELD_LENGTH_H, 0)) const {
        CPlayerTask task;
        
        // 创建射门任务
        TaskT t = task.makeTask(PlayerRole::GOALIE, TaskT::Shoot);
        t.player.pos = target;
        t.player.flag = (int)power; // 使用flag传递力量
        
        return t;
    }
    
    /**
     * @brief 创建传球任务
     * @param targetPos 目标位置
     * @param power 传球力量（0-100）
     * @return 创建的任务
     */
    PlayerTask createPassTask(const point2f& targetPos, double power = 80) const {
        CPlayerTask task;
        
        // 创建传球任务
        TaskT t = task.makeTask(PlayerRole::GOALIE, TaskT::Pass);
        t.player.pos = targetPos;
        t.player.flag = (int)power; // 力量
        
        return t;
    }
    
    /**
     * @brief 创建防守任务
     * @param targetPos 防守位置
     * @param faceBall 是否面向球
     * @return 创建的任务
     */
    PlayerTask createDefendTask(const point2f& targetPos, bool faceBall = true) const {
        CPlayerTask task;
        
        // 创建防守任务
        TaskT t = task.makeTask(PlayerRole::GOALIE, TaskT::SmartGoto);
        t.player.pos = targetPos;
     * @brief 估计到达目标所需时间
     * @param target 目标位置
     * @return 所需时间(秒)
     */
    double estimateTimeToTarget(const point2f& target) const {
        double dist = distanceTo(target);
        return dist / maxSpeed;
    }
};

/**
 * @brief 增强球员管理类，直接管理队伍中的所有球员
 */
class Players {
public:
    // 直接访问的成员变量
    Player player1;        // 第一个球员
    Player player2;        // 第二个球员
    Player* activePlayer;  // 指向当前活跃球员的指针
    
    // 球队整体信息
    point2f teamCentroid;                // 队伍质心
    double teamSpread;                   // 球员分散度
    bool hasPossession;                  // 队伍是否控球
    double formationWidth;               // 阵型宽度
    double formationDepth;               // 阵型深度
    
    // 球信息对象
    BallTools& ball;

    /**
     * @brief 构造函数
     * @param model 世界模型指针
     * @param ballTools 球工具类引用
     */
    Players(const WorldModel* model, BallTools& ballTools) : 
        model(model), 
        ball(ballTools),
        hasPossession(false),
        formationWidth(400),
        formationDepth(300) {
        
        updateState();
        last_update_time = std::chrono::high_resolution_clock::now();
    }
    
    ~Players() {}
    
    /**
     * @brief 更新所有球员状态
     * 每周期调用此函数更新信息
     */
    void updateState() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
        double dt = duration / 1000.0; // 转换为秒
        
        // 获取球员ID列表
        const bool* exists = model->get_our_exist_id();
        int goalie_id = model->get_our_goalie();
        
        // 重置球员数组
        activePlayers.clear();
        
        // 保存球员上一帧数据
        player1.lastPosition = player1.position;
        player1.lastVelocity = player1.velocity;
        player1.lastOrientation = player1.orientation;
        
        player2.lastPosition = player2.position;
        player2.lastVelocity = player2.velocity;
        player2.lastOrientation = player2.orientation;
        
        // 更新各球员信息
        bool foundPlayer1 = false;
        bool foundPlayer2 = false;
        
        for (int i = 0; i < MAX_TEAM_ROBOTS; ++i) {
            if (exists[i] && i != goalie_id) {
                Player* currentPlayer = nullptr;
                
                // 为第一个找到的非守门员球员赋值
                if (!foundPlayer1) {
                    player1.id = i;
                    player1.isActive = true;
                    currentPlayer = &player1;
                    foundPlayer1 = true;
                }
                // 为第二个找到的非守门员球员赋值
                else if (!foundPlayer2) {
                    player2.id = i;
                    player2.isActive = true;
                    currentPlayer = &player2;
                    foundPlayer2 = true;
                }
                
                if (currentPlayer) {
                    // 更新基础属性
                    currentPlayer->position = model->get_our_player_pos(i);
                    currentPlayer->velocity = model->get_our_player_v(i);
                    currentPlayer->orientation = model->get_our_player_dir(i);
                    currentPlayer->speed = currentPlayer->velocity.length();
                    
                    // 计算旋转速度
                    if (dt > 0) {
                        double angle_diff = anglemod(currentPlayer->orientation - currentPlayer->lastOrientation);
                        currentPlayer->rotationalSpeed = angle_diff / dt;
                    }
                    
                    // 更新历史数据
                    currentPlayer->updateHistory();
                    
                    // 检查是否持球
                    double dist = (ball.position - currentPlayer->position).length();
                    double angle_diff = fabs(anglemod((ball.position - currentPlayer->position).angle() - currentPlayer->orientation));
                    currentPlayer->hasBall = (dist < get_ball_threshold && angle_diff < M_PI/4);
                    
                    // 添加到活跃球员列表
                    activePlayers.push_back(currentPlayer);
                }
            }
        }
        
        // 未找到的球员设为不活跃
        if (!foundPlayer1) {
            player1.isActive = false;
        }
        if (!foundPlayer2) {
            player2.isActive = false;
        }
        
        // 更新团队信息
        updateTeamInfo();
        
        // 更新时间
        last_update_time = current_time;
    }
    
    /**
     * @brief 更新团队整体信息
     */
    void updateTeamInfo() {
        // 检查队伍是否有球权
        hasPossession = (player1.isActive && player1.hasBall) || 
                        (player2.isActive && player2.hasBall);
        
        // 计算队伍质心
        int activeCount = 0;
        point2f sum(0, 0);
        
        if (player1.isActive) {
            sum = sum + player1.position;
            activeCount++;
        }
        
        if (player2.isActive) {
            sum = sum + player2.position;
            activeCount++;
        }
        
        if (activeCount > 0) {
            teamCentroid = sum / activeCount;
        }
        
        // 计算球员分散度
        teamSpread = 0;
        if (player1.isActive && player2.isActive) {
            teamSpread = (player1.position - player2.position).length();
        }
    }
    
    /**
     * @brief 获取指定ID的球员
     * @param id 球员ID
     * @return 球员指针，若未找到则返回nullptr
     */
    Player* getPlayer(int id) {
        if (player1.isActive && player1.id == id) return &player1;
        if (player2.isActive && player2.id == id) return &player2;
        return nullptr;
    }
    
    /**
     * @brief 获取活跃球员列表
     * @return 活跃球员指针列表
     */
    const std::vector<Player*>& getActivePlayers() const {
        return activePlayers;
    }
    
    /**
     * @brief 获取最接近球的球员
     * @return 球员指针，若无球员则返回nullptr
     */
    Player* getClosestPlayerToBall() const {
        double min_dist = 9999.0;
        Player* closest = nullptr;
        
        for (Player* player : activePlayers) {
            double dist = (player->position - ball.position).length();
            if (dist < min_dist) {
                min_dist = dist;
                closest = player;
            }
        }
        
        return closest;
    }
    
    /**
     * @brief 获取最接近指定位置的球员
     * @param pos 目标位置
     * @return 球员指针，若无球员则返回nullptr
     */
    Player* getClosestPlayerToPosition(const point2f& pos) const {
        double min_dist = 9999.0;
        Player* closest = nullptr;
        
        for (Player* player : activePlayers) {
            double dist = (player->position - pos).length();
            if (dist < min_dist) {
                min_dist = dist;
                closest = player;
            }
        }
        
        return closest;
    }
    
    /**
     * @brief 获取持球的球员
     * @return 球员指针，若无则返回nullptr
     */
    Player* getBallPossessor() const {
        for (Player* player : activePlayers) {
            if (player->hasBall) return player;
        }
        return nullptr;
    }
    
    /**
     * @brief 获取最远离球的球员
     * @return 球员指针，若无球员则返回nullptr
     */
    Player* getFurthestPlayerFromBall() const {
        double max_dist = -1.0;
        Player* furthest = nullptr;
        
        for (Player* player : activePlayers) {
            double dist = (player->position - ball.position).length();
            if (dist > max_dist) {
                max_dist = dist;
                furthest = player;
            }
        }
        
        return furthest;
    }
    
    /**
     * @brief 计算最佳传球接收点
     * @param passer 传球者
     * @param receiver 接球者
     * @return 最佳接球点
     */
    point2f calculateBestReceivingPoint(Player* passer, Player* receiver) const {
        if (!passer || !receiver) return point2f(0, 0);
        
        // 基本传球方向
        point2f passDirection = receiver->position - passer->position;
        double passDistance = passDirection.length();
        
        if (passDistance < 0.1) return receiver->position;
        
        passDirection = passDirection / passDistance;
        
        // 计算接球者在传球路径上前进的位置
        double receiverRunTime = 1.0; // 接球者跑动时间，可调整
        point2f receiverFuturePos = receiver->position + receiver->velocity * receiverRunTime;
        
        // 考虑朝向球门的接球点
        point2f goalPos(FIELD_LENGTH_H, 0);
        point2f towardsGoal = (goalPos - receiverFuturePos).normalize();
        
        // 综合考虑当前位置、跑动和朝向球门的因素
        point2f bestReceivingPoint = receiverFuturePos + towardsGoal * 100; // 前进100单位
        
        return bestReceivingPoint;
    }
    
    /**
     * @brief 评估传球可能性
     * @param from 传球者位置
     * @param to 接球点位置
     * @return 传球成功概率(0-1)
     */
    double evaluatePassProbability(const point2f& from, const point2f& to) const {
        // 基础成功率
        double baseProbability = 0.8;
        
        // 根据距离调整
        double distance = (to - from).length();
        double distanceFactor = 1.0 - (distance / 1000.0); // 距离超过1000则概率为0
        distanceFactor = std::max(0.0, std::min(1.0, distanceFactor));
        
        // TODO: 考虑对手拦截的可能性
        double interceptionRisk = 0.1; // 暂时使用固定值
        
        return baseProbability * distanceFactor * (1.0 - interceptionRisk);
    }
    
    /**
     * @brief 创建移动到指定位置的任务
     * @param player 目标球员
     * @param target_pos 目标位置
     * @param target_dir 目标朝向，默认朝向球
     * @return 球员任务
     */
    PlayerTask createMoveTask(Player* player, const point2f& target_pos, double target_dir = -999) const {
        if (!player) return PlayerTask();
        
        PlayerTask task;
        
        // 设置目标位置
        task.target_pos = target_pos;
        
        // 如果没有指定朝向，默认朝向球
        if (target_dir < -998) {
            task.orientate = (ball.position - target_pos).angle();
        } else {
            task.orientate = target_dir;
        }
        
        // 设置运动参数
        task.maxAcceleration = 200;
        task.maxDeceleration = 200;
        
        return task;
    }
    
    /**
     * @brief 创建截球任务
     * @param player 目标球员
     * @param intercept_time 预计截球时间（秒）
     * @return 球员任务
     */
    PlayerTask createInterceptTask(Player* player, double intercept_time = 0.5) const {
        if (!player) return PlayerTask();
        
        point2f intercept_pos = ball.predictPosition(intercept_time);
        
        // 计算球的运动方向
        double ball_dir = ball.direction;
        
        // 创建截球任务，朝向与球的运动方向相反
        return createMoveTask(player, intercept_pos, ball_dir + M_PI);
    }
    
    /**
     * @brief 创建传球任务
     * @param passer 传球球员
     * @param receiver 接球球员
     * @param pass_power 传球力度，默认为3.0
     * @return 球员任务
     */
    PlayerTask createPassTask(Player* passer, Player* receiver, double pass_power = 3.0) const {
        if (!passer || !receiver) return PlayerTask();
        
        PlayerTask task;
        
        // 获取目标球员位置
        point2f target_pos = calculateBestReceivingPoint(passer, receiver);
        
        // 设置朝向目标球员
        task.target_pos = passer->position;  // 保持当前位置
        task.orientate = (target_pos - passer->position).angle();
        
        // 设置踢球参数
        task.needKick = true;
        task.isPass = true;
        task.isChipKick = false;  // 平射
        task.kickPower = pass_power;
        task.kickPrecision = 0.05;  // 较高的精度要求
        
        return task;
    }
    
    /**
     * @brief 创建射门任务
     * @param player 射门球员
     * @param chip_kick 是否挑射，默认为false（平射）
     * @param shoot_power 射门力度，默认为8.0
     * @return 球员任务
     */
    PlayerTask createShootTask(Player* player, bool chip_kick = false, double shoot_power = 8.0) const {
        if (!player) return PlayerTask();
        
        PlayerTask task;
        
        // 分析对方守门员位置，选择最佳射门点
        point2f goal_center(FIELD_LENGTH_H, 0);
        // TODO: 实现更智能的射门点选择
        
        // 设置朝向球门
        task.target_pos = player->position;  // 保持当前位置
        task.orientate = (goal_center - player->position).angle();
        
        // 设置踢球参数
        task.needKick = true;
        task.isPass = false;
        task.isChipKick = chip_kick;
        task.kickPower = shoot_power;
        task.kickPrecision = 0.02;  // 高精度射门
        
        if (chip_kick) {
            task.chipKickPower = shoot_power;
        }
        
        return task;
    }
    
    /**
     * @brief 创建运球任务
     * @param player 目标球员
     * @param target_pos 目标位置
     * @return 球员任务
     */
    PlayerTask createDribbleTask(Player* player, const point2f& target_pos) const {
        if (!player) return PlayerTask();
        
        PlayerTask task = createMoveTask(player, target_pos);
        
        // 设置持球参数
        task.needCb = true;  // 启用控球板
        
        return task;
    }
    
    /**
     * @brief 创建防守任务 - 防守特定区域
     * @param player 防守球员
     * @param zone_center 防守区域中心
     * @param facing_dir 面向方向，默认朝向球
     * @return 球员任务
     */
    PlayerTask createDefendZoneTask(Player* player, const point2f& zone_center, double facing_dir = -999) const {
        if (!player) return PlayerTask();
        
        // 如果球进入防守区域，则直接截球
        point2f player_target = zone_center;
        double ball_dist = (ball.position - zone_center).length();
        
        if (ball_dist < 150) { // 防守区域半径
            return createInterceptTask(player);
        }
        
        // 否则保持在防守位置
        return createMoveTask(player, player_target, facing_dir);
    }
    
    /**
     * @brief 获取适合的站位点
     * @param player 目标球员
     * @param isSupportingAttack 是否为进攻支援位置
     * @return 推荐站位点
     */
    point2f getRecommendedPosition(Player* player, bool isSupportingAttack = true) const {
        if (!player) return point2f(0, 0);
        
        // 获取持球队员
        Player* ballHandler = getBallPossessor();
        
        if (isSupportingAttack) {
            // 进攻支援位置：靠近前场，保持一定距离的支援位置
            point2f attackPos(FIELD_LENGTH_H * 0.5, 0);
            
            if (ballHandler) {
                // 根据持球队员位置调整支援点
                double support_dist = 300;
                point2f ballHandlerToGoal = (point2f(FIELD_LENGTH_H, 0) - ballHandler->position).normalize();
                point2f perpendicular(-ballHandlerToGoal.y, ballHandlerToGoal.x);
                
                // 选择远离防守方的一侧
                if (perpendicular.y < 0) perpendicular = perpendicular * -1;
                
                return ballHandler->position + ballHandlerToGoal * 100 + perpendicular * support_dist;
            }
            
            return attackPos;
        } else {
            // 防守位置：后场防守
            double defensiveX = -FIELD_LENGTH_H * 0.3; // 后场30%位置
            double defensiveY = 0;
            
            if (ball.position.x < 0) {
                // 如果球在我方半场，防守位置更靠近球门
                defensiveX = -FIELD_LENGTH_H * 0.6;
                defensiveY = ball.position.y * 0.5; // 部分跟随球的横向位置
            }
            
            return point2f(defensiveX, defensiveY);
        }
    }
    
    /**
     * @brief 判断球员是否在我方半场
     * @param player 目标球员
     * @return 是否在我方半场
     */
    bool isInOurHalf(Player* player) const {
        if (!player) return false;
        return player->position.x < 0;
    }

    /**
     * @brief 判断球员是否在对方半场
     * @param player 目标球员
     * @return 是否在对方半场
     */
    bool isInOpponentHalf(Player* player) const {
        if (!player) return false;
        return player->position.x > 0;
    }
    
    /**
     * @brief 检查球员是否在禁区内
     * @param player 目标球员
     * @param our_side 是否检查我方禁区
     * @return 是否在禁区内
     */
    bool isInPenaltyArea(Player* player, bool our_side = true) const {
        if (!player) return false;
        
        point2f penalty_center;
        
        if (our_side) {
            penalty_center = point2f(-FIELD_LENGTH_H, 0);
        } else {
            penalty_center = point2f(FIELD_LENGTH_H, 0);
        }
        
        return (player->position - penalty_center).length() < PENALTY_AREA_R;
    }

private:
    const WorldModel* model;
    std::vector<Player*> activePlayers;  // 活跃球员列表
    
    // 上次更新时间
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update_time;
};

#endif // PLAYERS_H 