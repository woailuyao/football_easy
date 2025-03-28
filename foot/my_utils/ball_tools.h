#ifndef BALL_TOOLS_H
#define BALL_TOOLS_H

#include <iostream>
#include <cmath>
#include <deque>
#include <chrono>
#include <vector>
#include <algorithm>
#include "../utils/ball.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "../utils/maths.h"

/**
 * @brief 增强球工具类，提供球的详细信息、历史数据和预测功能
 */
class BallTools {
public:
    // 当前球的状态信息
    point2f position;            // 当前位置
    point2f velocity;            // 当前速度向量
    double speed;                // 当前速度大小
    double direction;            // 当前方向（弧度）
    point2f acceleration;        // 当前加速度
    bool isControlled;           // 是否被控制
    
    // 历史状态跟踪
    std::deque<point2f> positionHistory;    // 位置历史
    std::deque<point2f> velocityHistory;    // 速度历史
    point2f lastPosition;                   // 上一帧位置
    point2f lastVelocity;                   // 上一帧速度
    double displacementRate;                // 位移变化率
    
    // 平滑数据
    point2f smoothedVelocity;               // 平滑处理后的速度

    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    BallTools(const WorldModel* model) : world_model(model) {
        // 初始化历史数据
        for (int i = 0; i < MAX_HISTORY_FRAMES; i++) {
            positionHistory.push_back(point2f(0, 0));
            velocityHistory.push_back(point2f(0, 0));
        }
        updateState();
        last_update_time = std::chrono::high_resolution_clock::now();
    }
    
    ~BallTools() {}

    /**
     * @brief 更新球的状态信息
     * 在每个周期调用此函数更新信息
     */
    void updateState() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
        double dt = duration / 1000.0; // 转换为秒
        
        // 保存前一帧数据
        lastPosition = position;
        lastVelocity = velocity;
        
        // 获取当前状态
        position = world_model->get_ball_pos();
        velocity = world_model->get_ball_vel();
        speed = velocity.length();
        direction = velocity.angle();
        
        // 更新历史位置队列
        positionHistory.pop_back();
        positionHistory.push_front(position);
        
        // 更新加速度估计
        if (dt > 0) {
            acceleration = (velocity - lastVelocity) / dt;
        }
        
        // 更新历史速度队列
        velocityHistory.pop_back();
        velocityHistory.push_front(velocity);
        
        // 更新速度平滑值
        smoothedVelocity = calculateSmoothedVelocity();
        
        // 计算是否被控制
        isControlled = checkIfControlled();
        
        // 更新位移变化率
        if (dt > 0) {
            displacementRate = (position - lastPosition).length() / dt;
        }
        
        // 更新时间
        last_update_time = current_time;
    }

    /**
     * @brief 获取球n帧前的位置
     * @param frames 帧数，0表示当前帧
     * @return n帧前的位置
     */
    point2f getPositionNFramesAgo(int frames) const {
        if (frames < 0 || frames >= positionHistory.size()) {
            return position; // 超出范围返回当前位置
        }
        return positionHistory[frames];
    }
    
    /**
     * @brief 获取球n帧前的速度向量
     * @param frames 帧数，0表示当前帧
     * @return n帧前的速度向量
     */
    point2f getVelocityNFramesAgo(int frames) const {
        if (frames < 0 || frames >= velocityHistory.size()) {
            return velocity; // 超出范围返回当前速度
        }
        return velocityHistory[frames];
    }
    
    /**
     * @brief 获取从n帧前到当前的平均速度向量
     * @param frames 帧数
     * @return 平均速度向量
     */
    point2f getAverageVelocity(int frames) const {
        if (frames <= 0 || frames >= positionHistory.size()) {
            return velocity;
        }
        
        point2f past_pos = getPositionNFramesAgo(frames);
        return (position - past_pos) / static_cast<float>(frames);
    }

    /**
     * @brief 预测球在指定时间后的位置
     * @param time 预测时间（秒）
     * @return 预测的球位置
     */
    point2f predictPosition(double time) const {
        // 包含摩擦力的物理模型预测
        double friction_coef = 0.03; // 摩擦系数，需要根据实际情况调整
        point2f result = position;
        
        if (speed < 1.0) {
            // 球几乎静止，不需要考虑运动
            return position;
        }
        
        // 运动方向
        point2f dir_vector(cos(direction), sin(direction));
        
        // 考虑摩擦力的减速模型
        double stopping_time = speed / friction_coef;
        
        if (time >= stopping_time) {
            // 如果预测时间超过球停止的时间，球将静止在最终位置
            double distance = 0.5 * speed * stopping_time;
            result = position + dir_vector * distance;
        } else {
            // 计算t时间内的位移
            double vt = speed - friction_coef * time;
            double distance = speed * time - 0.5 * friction_coef * time * time;
            result = position + dir_vector * distance;
        }
        
        return result;
    }
    
    /**
     * @brief 使用历史数据的高级预测模型
     * @param time 预测时间（秒）
     * @return 预测的球位置
     */
    point2f predictPositionAdvanced(double time) const {
        // 如果球几乎静止
        if (speed < 1.0) {
            return position;
        }
        
        // 计算过去几帧的加速度趋势
        point2f avg_accel = calculateAverageAcceleration(5);
        
        // 使用二阶运动学方程预测
        return position + 
               velocity * time + 
               avg_accel * (0.5f * time * time);  // 修复了操作符错误
    }
    
    /**
     * @brief 预测球将在何时到达指定位置
     * @param target 目标位置
     * @return 预计时间（秒），如果无法到达则返回-1
     */
    double predictTimeToPosition(const point2f& target) const {
        double friction_coef = 0.03; // 摩擦系数
        
        if (speed < 1.0) {
            // 球几乎静止，无法到达目标
            return -1;
        }
        
        // 计算球到目标的距离
        double distance = (target - position).length();
        
        // 计算球的最大行程
        double max_distance = 0.5 * speed * speed / friction_coef;
        
        if (distance > max_distance) {
            // 球的能量不足以到达目标
            return -1;
        }
        
        // 求解二次方程: s = v0*t - 0.5*friction_coef*t^2
        // 即: 0.5*friction_coef*t^2 - v0*t + s = 0
        double a = 0.5 * friction_coef;
        double b = -speed;
        double c = distance;
        
        // 计算判别式
        double delta = b*b - 4*a*c;
        
        if (delta < 0) {
            return -1; // 无解
        }
        
        // 计算两个解
        double t1 = (-b + sqrt(delta)) / (2*a);
        double t2 = (-b - sqrt(delta)) / (2*a);
        
        // 取正值且较小的解
        if (t1 > 0 && t2 > 0) {
            return std::min(t1, t2);
        } else if (t1 > 0) {
            return t1;
        } else if (t2 > 0) {
            return t2;
        }
        
        return -1; // 无正解
    }
    
    /**
     * @brief 计算截球点
     * @param robot_pos 机器人位置
     * @param robot_speed 机器人最大速度
     * @return 最佳截球点
     */
    point2f calculateInterceptPoint(const point2f& robot_pos, double robot_speed) const {
        // 如果球几乎静止，直接返回球的位置
        if (speed < 10.0) {
            return position;
        }
        
        // 简化计算：尝试不同的时间点，找到机器人能够到达的最早截球点
        double best_time = 0.0;
        point2f best_point = position;
        double min_arrival_diff = 9999.0;
        
        for (double t = 0.1; t < 3.0; t += 0.1) {
            // 球在t时刻的位置
            point2f ball_at_t = predictPosition(t);
            
            // 机器人到达该位置需要的时间
            double robot_dist = (ball_at_t - robot_pos).length();
            double robot_time = robot_dist / robot_speed;
            
            // 时间差异
            double time_diff = fabs(robot_time - t);
            
            if (time_diff < min_arrival_diff) {
                min_arrival_diff = time_diff;
                best_time = t;
                best_point = ball_at_t;
            }
        }
        
        return best_point;
    }

    /**
     * @brief 检查球是否在运动
     * @param speed_threshold 速度阈值，低于此值认为球静止
     * @return 是否在运动
     */
    bool isMoving(double speed_threshold = 10.0) const {
        return speed > speed_threshold;
    }

    /**
     * @brief 检查球是否在指定区域内
     * @param area_center 区域中心
     * @param radius 区域半径
     * @return 是否在区域内
     */
    bool isInArea(const point2f& area_center, double radius) const {
        return (position - area_center).length() < radius;
    }
    
    /**
     * @brief 检查球是否在矩形区域内
     * @param center 矩形中心
     * @param width 矩形宽度
     * @param height 矩形高度
     * @return 是否在区域内
     */
    bool isInRectangle(const point2f& center, double width, double height) const {
        return (fabs(position.x - center.x) < width/2.0 && 
                fabs(position.y - center.y) < height/2.0);
    }

    /**
     * @brief 检查球是否在我方半场
     * @return 是否在我方半场
     */
    bool isInOurHalf() const {
        return position.x < 0;
    }

    /**
     * @brief 检查球是否在对方半场
     * @return 是否在对方半场
     */
    bool isInOpponentHalf() const {
        return position.x > 0;
    }

    /**
     * @brief 检查球是否在禁区内
     * @param our_side 是否检查我方禁区，默认为true
     * @return 是否在禁区内
     */
    bool isInPenaltyArea(bool our_side = true) const {
        point2f penalty_center;
        if (our_side) {
            penalty_center = point2f(-FIELD_LENGTH_H, 0);
        } else {
            penalty_center = point2f(FIELD_LENGTH_H, 0);
        }
        
        double distance = (position - penalty_center).length();
        return distance < PENALTY_AREA_R;
    }
    
    /**
     * @brief 检查球是否在场地内
     * @param margin 场地边缘余量
     * @return 是否在场地内
     */
    bool isInField(double margin = 10.0) const {
        return (fabs(position.x) < FIELD_LENGTH_H - margin && 
                fabs(position.y) < FIELD_WIDTH_H - margin);
    }

    /**
     * @brief 计算球到指定点的距离
     * @param target 目标点
     * @return 距离
     */
    double distanceTo(const point2f& target) const {
        return (position - target).length();
    }

    /**
     * @brief 计算球到指定点的朝向角度
     * @param target 目标点
     * @return 角度（弧度）
     */
    double directionTo(const point2f& target) const {
        return (target - position).angle();
    }
    
    /**
     * @brief 计算球到直线的最短距离
     * @param line_start 直线起点
     * @param line_end 直线终点
     * @return 最短距离
     */
    double distanceToLine(const point2f& line_start, const point2f& line_end) const {
        point2f line_vec = line_end - line_start;
        double line_len = line_vec.length();
        
        if (line_len < 0.001) {
            return (position - line_start).length();
        }
        
        point2f norm_line = line_vec / line_len;
        point2f to_ball = position - line_start;
        double proj = to_ball.x * norm_line.x + to_ball.y * norm_line.y;
        
        if (proj <= 0) {
            return (position - line_start).length();
        }
        
        if (proj >= line_len) {
            return (position - line_end).length();
        }
        
        point2f closest = line_start + norm_line * proj;
        return (position - closest).length();
    }

    /**
     * @brief 判断球是否朝向某个方向移动
     * @param direction 方向（弧度）
     * @param tolerance 容差角度（弧度）
     * @return 是否朝向该方向
     */
    bool isMovingTowards(double direction, double tolerance = M_PI/8) const {
        if (!isMoving()) return false;
        
        double angle_diff = fabs(anglemod(direction - direction));
        return angle_diff < tolerance;
    }
    
    /**
     * @brief 判断球是否朝向某个目标移动
     * @param target 目标位置
     * @param tolerance 容差角度（弧度）
     * @return 是否朝向该目标
     */
    bool isMovingTowardsTarget(const point2f& target, double tolerance = M_PI/8) const {
        if (!isMoving()) return false;
        
        double target_direction = directionTo(target);
        return isMovingTowards(target_direction, tolerance);
    }
    
    /**
     * @brief 判断球是否远离某个目标
     * @param target 目标位置
     * @param tolerance 容差角度（弧度）
     * @return 是否远离该目标
     */
    bool isMovingAwayFromTarget(const point2f& target, double tolerance = M_PI/8) const {
        if (!isMoving()) return false;
        
        double target_direction = directionTo(target);
        double angle_diff = fabs(anglemod(direction - target_direction));
        return angle_diff > (M_PI - tolerance);
    }
    
    /**
     * @brief 判断球是否正在被某个球员控制
     * @return 是否被控制
     */
    bool isBallControlled() const {
        return isControlled;
    }
    
    /**
     * @brief 获取控制球的球员ID
     * @param is_our_team 返回是否是我方球员
     * @return 控制球的球员ID，如果没有则返回-1
     */
    int getControllingPlayerId(bool& is_our_team) const {
        if (!isControlled) {
            is_our_team = false;
            return -1;
        }
        
        const bool* our_exists = world_model->get_our_exist_id();
        const bool* opp_exists = world_model->get_opp_exist_id();
        
        // 检查我方球员
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (our_exists[i]) {
                point2f player_pos = world_model->get_our_player_pos(i);
                double player_dir = world_model->get_our_player_dir(i);
                
                if (isPlayerControllingBall(player_pos, player_dir)) {
                    is_our_team = true;
                    return i;
                }
            }
        }
        
        // 检查对方球员
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (opp_exists[i]) {
                point2f player_pos = world_model->get_opp_player_pos(i);
                double player_dir = world_model->get_opp_player_dir(i);
                
                if (isPlayerControllingBall(player_pos, player_dir)) {
                    is_our_team = false;
                    return i;
                }
            }
        }
        
        is_our_team = false;
        return -1;
    }
    
    /**
     * @brief 获取球历史位置
     * @return 球历史位置队列
     */
    const std::deque<point2f>& getPositionHistory() const {
        return positionHistory;
    }
    
    /**
     * @brief 获取球历史速度
     * @return 球历史速度队列
     */
    const std::deque<point2f>& getVelocityHistory() const {
        return velocityHistory;
    }
    
    /**
     * @brief 判断球是否即将出界
     * @param time_horizon 预测时间范围（秒）
     * @return 是否即将出界
     */
    bool isGoingOutOfBounds(double time_horizon = 1.0) const {
        // 预测未来位置
        point2f future_pos = predictPosition(time_horizon);
        
        // 检查是否出界
        return (fabs(future_pos.x) > FIELD_LENGTH_H || 
                fabs(future_pos.y) > FIELD_WIDTH_H);
    }
    
    /**
     * @brief 判断球是否向我方球门移动
     * @param tolerance 角度容差（弧度）
     * @return 是否向我方球门移动
     */
    bool isMovingTowardsOurGoal(double tolerance = M_PI/6) const {
        point2f our_goal(-FIELD_LENGTH_H, 0);
        return isMovingTowardsTarget(our_goal, tolerance);
    }
    
    /**
     * @brief 判断球是否向对方球门移动
     * @param tolerance 角度容差（弧度）
     * @return 是否向对方球门移动
     */
    bool isMovingTowardsOpponentGoal(double tolerance = M_PI/6) const {
        point2f opponent_goal(FIELD_LENGTH_H, 0);
        return isMovingTowardsTarget(opponent_goal, tolerance);
    }
    
    /**
     * @brief 预测球的轨迹点
     * @param num_points 要预测的点数
     * @param time_step 每点时间间隔
     * @return 预测轨迹点列表
     */
    std::vector<point2f> predictTrajectory(int num_points = 10, double time_step = 0.1) const {
        std::vector<point2f> trajectory;
        
        for (int i = 0; i < num_points; i++) {
            double time = (i + 1) * time_step;
            trajectory.push_back(predictPosition(time));
        }
        
        return trajectory;
    }
    
    /**
     * @brief 判断某球员是否可能截到球
     * @param player_pos 球员位置
     * @param player_speed 球员最大速度
     * @param time_horizon 时间范围（秒）
     * @return 是否可能截球
     */
    bool canPlayerInterceptBall(const point2f& player_pos, double player_speed, double time_horizon = 3.0) const {
        // 球几乎静止，直接判断距离
        if (speed < 10.0) {
            double dist_to_ball = (player_pos - position).length();
            double time_to_reach = dist_to_ball / player_speed;
            return time_to_reach <= time_horizon;
        }
        
        // 对时间区间进行采样
        double time_step = 0.1;
        int num_samples = static_cast<int>(time_horizon / time_step) + 1;
        
        for (int i = 0; i < num_samples; i++) {
            double t = i * time_step;
            point2f ball_pos_at_t = predictPosition(t);
            
            double dist_to_ball = (player_pos - ball_pos_at_t).length();
            double time_to_reach = dist_to_ball / player_speed;
            
            if (time_to_reach <= t) {
                return true;  // 球员可以在t时刻到达球的位置
            }
        }
        
        return false;  // 整个时间范围内球员都无法截到球
    }

private:
    const WorldModel* world_model;
    static const int MAX_HISTORY_FRAMES = 60; // 1秒的历史(假设60fps)
    
    // 上次更新时间
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update_time;
    
    /**
     * @brief 计算平滑的速度向量
     * @return 平滑后的速度向量
     */
    point2f calculateSmoothedVelocity() const {
        const int SMOOTH_WINDOW = 5;
        if (velocityHistory.size() < SMOOTH_WINDOW) {
            return velocity;
        }
        
        point2f sum(0, 0);
        for (int i = 0; i < SMOOTH_WINDOW; i++) {
            sum = sum + velocityHistory[i];
        }
        return sum / SMOOTH_WINDOW;
    }
    
    /**
     * @brief 计算平均加速度
     * @param frames 要考虑的帧数
     * @return 平均加速度
     */
    point2f calculateAverageAcceleration(int frames) const {
        if (frames <= 1 || velocityHistory.size() < frames) {
            return acceleration;
        }
        
        return (velocityHistory[0] - velocityHistory[frames-1]) / static_cast<float>(frames);
    }
    
    /**
     * @brief 检查特定球员是否控制球
     * @param player_pos 球员位置
     * @param player_dir 球员朝向
     * @return 是否控制球
     */
    bool isPlayerControllingBall(const point2f& player_pos, double player_dir) const {
        // 计算球到球员的距离
        double dist = (position - player_pos).length();
        if (dist > 20.0) return false;
        
        // 检查球是否在球员前方
        point2f player_front_dir(cos(player_dir), sin(player_dir));
        point2f to_ball = position - player_pos;
        double dot_product = to_ball.x * player_front_dir.x + to_ball.y * player_front_dir.y;
        
        return dot_product > 0;
    }
    
    /**
     * @brief 检查球是否被某个球员控制
     * @return 是否被控制
     */
    bool checkIfControlled() const {
        const bool* our_exists = world_model->get_our_exist_id();
        const bool* opp_exists = world_model->get_opp_exist_id();
        
        // 检查球是否被我方球员控制
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (our_exists[i]) {
                point2f player_pos = world_model->get_our_player_pos(i);
                double player_dir = world_model->get_our_player_dir(i);
                
                if (isPlayerControllingBall(player_pos, player_dir)) {
                    return true;
                }
            }
        }
        
        // 检查球是否被对方球员控制
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (opp_exists[i]) {
                point2f player_pos = world_model->get_opp_player_pos(i);
                double player_dir = world_model->get_opp_player_dir(i);
                
                if (isPlayerControllingBall(player_pos, player_dir)) {
                    return true;
                }
            }
        }
        
        // 球没有被控制
        return false;
    }
};

#endif // BALL_TOOLS_H