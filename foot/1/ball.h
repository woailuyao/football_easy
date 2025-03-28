#ifndef BALL_H
#define BALL_H

#include <iostream>
#include "../utils/vector.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/maths.h"

// 球信息类 - 管理球的信息和提供相关工具函数
class Ball {
private:
    const WorldModel* model;       // 世界模型指针
    point2f current_pos;           // 当前位置
    point2f last_pos;              // 上一帧位置
    point2f velocity;              // 速度
    float speed;                   // 速度大小
    
public:
    // 构造函数
    Ball(const WorldModel* world_model) 
        : model(world_model) {
        update();
    }
    
    // 更新球信息
    void update() {
        current_pos = model->get_ball_pos();
        last_pos = model->get_ball_pos(1);
        velocity = model->get_ball_vel();
        speed = velocity.length();
    }
    
    // 获取球的当前位置
    const point2f& getPosition() const {
        return current_pos;
    }
    
    // 获取球的上一帧位置
    const point2f& getLastPosition() const {
        return last_pos;
    }
    
    // 获取球的速度向量
    const point2f& getVelocity() const {
        return velocity;
    }
    
    // 获取球的速度大小
    float getSpeed() const {
        return speed;
    }
    
    // 获取球的移动方向
    float getDirection() const {
        if (speed > 0.1f) {
            return velocity.angle();
        }
        return 0.0f; // 球几乎静止时返回0
    }
    
    // 判断球是否静止
    bool isStationary(float threshold = 5.0f) const {
        return speed < threshold;
    }
    
    // 预测未来位置
    point2f predictPosition(float time_ahead = 0.5f) const {
        // 考虑球的减速
        float decel_factor = 0.8f; // 球的减速因子
        point2f avg_vel = velocity * (1.0f - decel_factor * time_ahead / 2.0f);
        
        // 线性预测
        return current_pos + avg_vel * time_ahead;
    }
    
    // 计算截球点
    point2f calculateInterceptPoint(const point2f& robot_pos, float robot_speed) const {
        // 如果球几乎静止，直接返回球的位置
        if (isStationary()) {
            return current_pos;
        }
        
        // 简化计算：尝试不同的时间点，找到机器人能够到达的最早截球点
        float best_time = 0.0f;
        point2f best_point = current_pos;
        float min_arrival_diff = 9999.0f;
        
        for (float t = 0.1f; t < 3.0f; t += 0.1f) {
            // 球在t时刻的位置
            point2f ball_at_t = predictPosition(t);
            
            // 机器人到达该位置需要的时间
            float robot_dist = (ball_at_t - robot_pos).length();
            float robot_time = robot_dist / robot_speed;
            
            // 时间差异
            float time_diff = fabs(robot_time - t);
            
            if (time_diff < min_arrival_diff) {
                min_arrival_diff = time_diff;
                best_time = t;
                best_point = ball_at_t;
            }
        }
        
        return best_point;
    }
    
    // 判断球是否正在向目标移动
    bool isMovingToward(const point2f& target, float angle_tolerance = M_PI/6) const {
        if (isStationary()) return false;
        
        float ball_dir = getDirection();
        float target_dir = (target - current_pos).angle();
        
        return fabs(anglemod(ball_dir - target_dir)) < angle_tolerance;
    }
    
    // 判断球是否正在远离目标
    bool isMovingAway(const point2f& target, float angle_tolerance = M_PI/6) const {
        if (isStationary()) return false;
        
        float ball_dir = getDirection();
        float target_dir = (target - current_pos).angle();
        
        return fabs(anglemod(ball_dir - target_dir)) > (M_PI - angle_tolerance);
    }
    
    // 计算球到点的距离
    float distanceTo(const point2f& target) const {
        return (current_pos - target).length();
    }
    
    // 计算球到线的距离
    float distanceToLine(const point2f& line_start, const point2f& line_end) const {
        point2f line_vec = line_end - line_start;
        float line_len = line_vec.length();
        
        if (line_len < 0.001f) return (current_pos - line_start).length();
        
        point2f norm_line = line_vec / line_len;
        point2f to_ball = current_pos - line_start;
        float proj = to_ball.x * norm_line.x + to_ball.y * norm_line.y;
        
        if (proj <= 0) return (current_pos - line_start).length();
        if (proj >= line_len) return (current_pos - line_end).length();
        
        point2f closest = line_start + norm_line * proj;
        return (current_pos - closest).length();
    }
    
    // 判断球是否处于某个区域内
    bool isInArea(const point2f& area_center, float half_width, float half_height) const {
        return (fabs(current_pos.x - area_center.x) <= half_width && 
                fabs(current_pos.y - area_center.y) <= half_height);
    }
    
    // 判断球是否在场地内
    bool isInField() const {
        return (fabs(current_pos.x) < FIELD_LENGTH_H - 10 && 
                fabs(current_pos.y) < FIELD_WIDTH_H - 10);
    }
    
    // 判断谁将更快到达球
    bool canReachBallFirst(const point2f& our_pos, const point2f& opp_pos, 
                          float our_speed, float opp_speed) const {
        float our_dist = (current_pos - our_pos).length();
        float opp_dist = (current_pos - opp_pos).length();
        
        float our_time = our_dist / our_speed;
        float opp_time = opp_dist / opp_speed;
        
        return our_time <= opp_time;
    }
    
    // 判断是否能够在指定时间内抢到球
    bool canReachBallFirst(int robot_id, float robot_speed, float max_time = 3.0f) const {
        const point2f& robot_pos = model->get_our_player_pos(robot_id);
        
        // 我方到球的时间
        float our_dist = (current_pos - robot_pos).length();
        float our_time = our_dist / robot_speed;
        
        if (our_time > max_time) return false; // 如果时间太长，放弃
        
        // 检查对手是否能更快到达
        bool can_reach_first = true;
        const bool* opp_exist_id = model->get_opp_exist_id();
        
        for (int i = 0; i < MAX_ROBOTS; i++) {
            if (opp_exist_id[i]) {
                point2f opp_pos = model->get_opp_player_pos(i);
                float opp_dist = (current_pos - opp_pos).length();
                float opp_time = opp_dist / robot_speed; // 假设对手速度与我方相同
                
                if (opp_time < our_time) {
                    can_reach_first = false;
                    break;
                }
            }
        }
        
        return can_reach_first;
    }
};

#endif // BALL_H 