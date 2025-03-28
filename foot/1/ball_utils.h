#ifndef BALL_UTILS_H
#define BALL_UTILS_H

#include <iostream>
#include "../utils/vector.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/maths.h"

// 球工具类 - 提供与球相关的辅助功能
class BallUtils {
public:
    // 预测球的未来位置
    // time_ahead: 预测未来多少秒的位置
    static point2f predict_ball_pos(const WorldModel* model, float time_ahead = 0.5f) {
        const point2f& ball_pos = model->get_ball_pos();
        const point2f& ball_vel = model->get_ball_vel();
        
        // 考虑球的减速
        float decel_factor = 0.8f; // 球的减速因子，可以根据实际情况调整
        point2f avg_vel = ball_vel * (1.0f - decel_factor * time_ahead / 2.0f);
        
        // 线性预测
        return ball_pos + avg_vel * time_ahead;
    }
    
    // 计算最佳截球点
    static point2f intercept_point(const WorldModel* model, const point2f& robot_pos, float robot_speed) {
        const point2f& ball_pos = model->get_ball_pos();
        const point2f& ball_vel = model->get_ball_vel();
        
        // 如果球几乎静止，直接返回球的位置
        if (ball_vel.length() < 5.0f) {
            return ball_pos;
        }
        
        // 计算截球时间
        float ball_speed = ball_vel.length();
        point2f ball_dir = ball_vel / ball_speed;
        
        // 简化计算：尝试不同的时间点，找到机器人能够到达的最早截球点
        float best_time = 0.0f;
        point2f best_point = ball_pos;
        float min_arrival_diff = 9999.0f;
        
        for (float t = 0.1f; t < 3.0f; t += 0.1f) {
            // 球在t时刻的位置
            point2f ball_at_t = predict_ball_pos(model, t);
            
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
    static bool ball_moving_toward(const WorldModel* model, const point2f& target, float angle_tolerance = M_PI/6) {
        const point2f& ball_pos = model->get_ball_pos();
        const point2f& ball_vel = model->get_ball_vel();
        
        if (ball_vel.length() < 5.0f) return false; // 球几乎静止
        
        float ball_dir = ball_vel.angle();
        float target_dir = (target - ball_pos).angle();
        
        return fabs(anglemod(ball_dir - target_dir)) < angle_tolerance;
    }
    
    // 判断是否能够在指定时间内抢到球
    static bool can_reach_ball_first(const WorldModel* model, int robot_id, float robot_speed, float max_time = 3.0f) {
        const point2f& ball_pos = model->get_ball_pos();
        const point2f& robot_pos = model->get_our_player_pos(robot_id);
        
        // 我方到球的时间
        float our_dist = (ball_pos - robot_pos).length();
        float our_time = our_dist / robot_speed;
        
        if (our_time > max_time) return false; // 如果时间太长，放弃
        
        // 检查对手是否能更快到达
        bool can_reach_first = true;
        const bool* opp_exist_id = model->get_opp_exist_id();
        
        for (int i = 0; i < MAX_ROBOTS; i++) {
            if (opp_exist_id[i]) {
                point2f opp_pos = model->get_opp_player_pos(i);
                float opp_dist = (ball_pos - opp_pos).length();
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

#endif // BALL_UTILS_H 