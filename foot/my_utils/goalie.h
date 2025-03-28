#ifndef GOALIE_H
#define GOALIE_H

#include <iostream>
#include <cmath>
#include "../utils/robot.h"
#include "../utils/WorldModel.h"
#include "../utils/PlayerTask.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "../utils/maths.h"
#include "ball_tools.h"

/**
 * @brief 我方守门员工具类，提供守门员相关信息和操作方法
 */
class Goalie {
public:
    Goalie(const WorldModel* model) : world_model(model), ball_tools(model) {}
    ~Goalie() {}

    /**
     * @brief 获取守门员ID
     * @return 守门员ID
     */
    int getId() const {
        return world_model->get_our_goalie();
    }

    /**
     * @brief 获取守门员位置
     * @return 守门员位置
     */
    point2f getPosition() const {
        return world_model->get_our_player_pos(getId());
    }

    /**
     * @brief 获取守门员朝向
     * @return 守门员朝向（弧度）
     */
    double getOrientation() const {
        return world_model->get_our_player_dir(getId());
    }

    /**
     * @brief 获取守门员速度向量
     * @return 守门员速度向量
     */
    point2f getVelocity() const {
        return world_model->get_our_player_v(getId());
    }

    /**
     * @brief 获取我方球门中心坐标
     * @return 球门中心坐标
     */
    point2f getGoalCenter() const {
        return point2f(-FIELD_LENGTH_H, 0);
    }

    /**
     * @brief 计算球到球门的距离
     * @return 距离
     */
    double ballToGoalDistance() const {
        return (ball_tools.getPosition() - getGoalCenter()).length();
    }

    /**
     * @brief 计算球到守门员的距离
     * @return 距离
     */
    double ballToGoalieDistance() const {
        return (ball_tools.getPosition() - getPosition()).length();
    }

    /**
     * @brief 检查球是否朝向球门
     * @param angle_threshold 角度阈值（弧度），默认为M_PI/8
     * @return 是否朝向球门
     */
    bool isBallMovingTowardsGoal(double angle_threshold = M_PI/8) const {
        if (!ball_tools.isMoving()) return false;
        
        point2f ball_pos = ball_tools.getPosition();
        point2f goal_center = getGoalCenter();
        
        // 计算球到球门的方向
        double goal_dir = (goal_center - ball_pos).angle();
        
        // 计算球的移动方向与球门方向的角度差
        double ball_dir = ball_tools.getDirection();
        double angle_diff = fabs(anglemod(ball_dir - goal_dir));
        
        return angle_diff < angle_threshold;
    }

    /**
     * @brief 预测球可能的入球点
     * @return 预测的入球点坐标
     */
    point2f predictGoalLine() const {
        point2f ball_pos = ball_tools.getPosition();
        point2f ball_vel = ball_tools.getVelocity();
        
        if (ball_vel.length() < 10) {
            // 球速太慢，无法准确预测
            return point2f(-FIELD_LENGTH_H, 0);
        }
        
        // 计算球的运动直线方程: y = m*x + b
        double m = ball_vel.y / ball_vel.x;  // 斜率
        if (fabs(m) > 100) {
            // 接近垂直运动，直接使用当前y坐标
            return point2f(-FIELD_LENGTH_H, ball_pos.y);
        }
        
        double b = ball_pos.y - m * ball_pos.x;  // 截距
        
        // 计算该直线与球门线的交点
        double goal_line_x = -FIELD_LENGTH_H;
        double goal_line_y = m * goal_line_x + b;
        
        // 确保交点在合理范围内（球门宽度限制）
        if (goal_line_y > GOAL_WIDTH_H) {
            goal_line_y = GOAL_WIDTH_H;
        } else if (goal_line_y < -GOAL_WIDTH_H) {
            goal_line_y = -GOAL_WIDTH_H;
        }
        
        return point2f(goal_line_x, goal_line_y);
    }

    /**
     * @brief 创建基本防守位置任务
     * @return 守门员任务
     */
    PlayerTask createDefendTask() const {
        PlayerTask task;
        int goalie_id = getId();
        
        // 获取球的位置和速度
        point2f ball_pos = ball_tools.getPosition();
        
        // 计算最佳防守位置
        point2f defend_pos;
        
        if (isBallMovingTowardsGoal() && ball_tools.getSpeed() > 100) {
            // 如果球高速向球门移动，预测入球点并防守
            defend_pos = predictGoalLine();
            defend_pos.x += 10;  // 稍微离开球门线
        } else {
            // 常规防守位置，基于球的位置设置防守点
            point2f goal_center = getGoalCenter();
            
            // 计算球到球门中心的方向向量
            point2f ball_to_goal = goal_center - ball_pos;
            double dist = ball_to_goal.length();
            
            // 归一化向量并设置守门员位置
            ball_to_goal = ball_to_goal / dist;
            
            // 守门员位置在球门前方，距离球门线约20厘米
            defend_pos = goal_center + ball_to_goal * 20;
            
            // 限制防守位置不要超出球门宽度太多
            if (defend_pos.y > GOAL_WIDTH_H + 10) {
                defend_pos.y = GOAL_WIDTH_H + 10;
            } else if (defend_pos.y < -GOAL_WIDTH_H - 10) {
                defend_pos.y = -GOAL_WIDTH_H - 10;
            }
            
            // 确保守门员不会离开球门太远
            if (defend_pos.x > -FIELD_LENGTH_H + 30) {
                defend_pos.x = -FIELD_LENGTH_H + 30;
            }
        }
        
        // 设置防守位置和朝向球
        task.target_pos = defend_pos;
        task.orientate = (ball_pos - defend_pos).angle();
        
        // 设置运动参数
        task.maxAcceleration = 300;  // 守门员加速可以更快
        task.maxDeceleration = 300;
        
        return task;
    }

    /**
     * @brief 创建紧急出击任务
     * @return 守门员任务
     */
    PlayerTask createEmergencyTask() const {
        PlayerTask task;
        int goalie_id = getId();
        
        // 获取球的位置
        point2f ball_pos = ball_tools.getPosition();
        point2f goal_center = getGoalCenter();
        
        // 计算出击位置，向球的方向移动
        point2f intercept_pos = ball_pos;
        
        // 限制出击范围，不要离开禁区太远
        if (intercept_pos.x < -FIELD_LENGTH_H + DEFENSE_DEPTH) {
            // 如果球在禁区内部，直接移动到球的位置
            task.target_pos = intercept_pos;
        } else {
            // 否则，在禁区边缘截球
            double t = (-FIELD_LENGTH_H + DEFENSE_DEPTH - goal_center.x) / 
                    (ball_pos.x - goal_center.x);
            
            if (t > 0 && t < 1) {
                intercept_pos = goal_center + (ball_pos - goal_center) * t;
            } else {
                // 保持在禁区边缘
                intercept_pos.x = -FIELD_LENGTH_H + DEFENSE_DEPTH;
            }
            
            task.target_pos = intercept_pos;
        }
        
        // 朝向球
        task.orientate = (ball_pos - task.target_pos).angle();
        
        // 设置任务参数
        task.maxAcceleration = 400;  // 紧急情况下最大加速
        task.maxDeceleration = 400;
        
        // 如果球足够近，可以尝试踢出去
        if (ballToGoalieDistance() < get_ball_threshold) {
            task.needKick = true;
            task.kickPower = 8.0;  // 全力踢出
            task.isChipKick = true;  // 挑球更安全
            task.chipKickPower = 8.0;
        }
        
        return task;
    }

    /**
     * @brief 检查是否需要紧急出击
     * @return 是否需要紧急出击
     */
    bool needEmergency() const {
        // 如果球在禁区内且速度较慢，可能需要出击
        return ball_tools.isInPenaltyArea(true) && 
               ball_tools.getSpeed() < 50 && 
               ballToGoalDistance() < 150;
    }

    /**
     * @brief 决定守门员最佳任务
     * @return 守门员任务
     */
    PlayerTask decideGoalieTask() const {
        if (needEmergency()) {
            return createEmergencyTask();
        } else {
            return createDefendTask();
        }
    }

private:
    const WorldModel* world_model;
    BallTools ball_tools;
};

#endif // GOALIE_H 