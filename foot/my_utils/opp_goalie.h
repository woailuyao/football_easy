#ifndef OPP_GOALIE_H
#define OPP_GOALIE_H

#include <iostream>
#include <cmath>
#include "../utils/robot.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "../utils/maths.h"
#include "ball_tools.h"

/**
 * @brief 敌方守门员工具类，提供敌方守门员相关信息和分析方法
 */
class OppGoalie {
public:
    OppGoalie(const WorldModel* model) : world_model(model), ball_tools(model) {}
    ~OppGoalie() {}

    /**
     * @brief 获取敌方守门员ID
     * @return 守门员ID
     */
    int getId() const {
        return world_model->get_opp_goalie();
    }

    /**
     * @brief 获取敌方守门员位置
     * @return 守门员位置
     */
    point2f getPosition() const {
        return world_model->get_opp_player_pos(getId());
    }

    /**
     * @brief 获取敌方守门员朝向
     * @return 守门员朝向（弧度）
     */
    double getOrientation() const {
        return world_model->get_opp_player_dir(getId());
    }

    /**
     * @brief 获取敌方球门中心坐标
     * @return 球门中心坐标
     */
    point2f getGoalCenter() const {
        return point2f(FIELD_LENGTH_H, 0);
    }

    /**
     * @brief 计算敌方守门员到球的距离
     * @return 距离
     */
    double distanceToBall() const {
        return (getPosition() - ball_tools.getPosition()).length();
    }

    /**
     * @brief 计算敌方守门员到敌方球门的距离
     * @return 距离
     */
    double distanceToGoal() const {
        return (getPosition() - getGoalCenter()).length();
    }

    /**
     * @brief 检查敌方守门员是否在禁区内
     * @return 是否在禁区内
     */
    bool isInPenaltyArea() const {
        point2f penalty_center = getGoalCenter();
        double distance = (getPosition() - penalty_center).length();
        return distance < PENALTY_AREA_R;
    }

    /**
     * @brief 计算敌方守门员是否正面对着球
     * @param angle_threshold 角度阈值（弧度），默认为M_PI/6
     * @return 是否面对着球
     */
    bool isFacingBall(double angle_threshold = M_PI/6) const {
        point2f goalie_pos = getPosition();
        double goalie_dir = getOrientation();
        point2f ball_pos = ball_tools.getPosition();
        
        // 计算守门员到球的方向
        point2f to_ball = ball_pos - goalie_pos;
        double ball_angle = to_ball.angle();
        
        // 计算朝向与球的角度差
        double angle_diff = fabs(anglemod(ball_angle - goalie_dir));
        
        return angle_diff < angle_threshold;
    }

    /**
     * @brief 判断敌方守门员是否可能出击
     * @return 是否可能出击
     */
    bool mayRushOut() const {
        point2f ball_pos = ball_tools.getPosition();
        
        // 如果球在敌方半场且接近禁区且守门员朝向球，可能出击
        return ball_pos.x > 0 && 
               ball_pos.x > FIELD_LENGTH_H - DEFENSE_DEPTH - 50 && 
               distanceToBall() < 150 && 
               isFacingBall();
    }

    /**
     * @brief 评估敌方守门员防守水平
     * 较低的值表示防守位置较差，可能有射门机会
     * @return 防守评分 (0-10)
     */
    double evaluateDefensivePosition() const {
        point2f goalie_pos = getPosition();
        point2f goal_center = getGoalCenter();
        point2f ball_pos = ball_tools.getPosition();
        
        // 如果球不在敌方半场，守门员防守水平假定为较高
        if (ball_pos.x < 0) {
            return 8.0;
        }
        
        // 计算球到球门的射门角度
        point2f goal_to_ball = ball_pos - goal_center;
        double shooting_angle = goal_to_ball.angle();
        
        // 计算理想防守位置 (基于球和球门的连线)
        point2f ideal_pos = goal_center;
        ideal_pos.x -= 20;  // 离球门线稍微有点距离
        
        if (goal_to_ball.length() > 50) {
            // 根据球的位置计算理想的y坐标
            double ratio = 30.0 / goal_to_ball.length();
            ideal_pos.y = goal_center.y + goal_to_ball.y * ratio;
            
            // 限制在球门宽度范围内
            if (ideal_pos.y > GOAL_WIDTH_H) {
                ideal_pos.y = GOAL_WIDTH_H;
            } else if (ideal_pos.y < -GOAL_WIDTH_H) {
                ideal_pos.y = -GOAL_WIDTH_H;
            }
        }
        
        // 计算守门员与理想位置的偏差
        double position_error = (goalie_pos - ideal_pos).length();
        
        // 计算防守评分 (满分10分，误差越大分数越低)
        double score = 10.0 - position_error / 10.0;
        if (score < 0) score = 0;
        if (score > 10) score = 10;
        
        // 如果守门员不面向球，额外扣分
        if (!isFacingBall()) {
            score -= 2.0;
        }
        
        // 如果守门员离球门太远，额外扣分
        if (distanceToGoal() > 50) {
            score -= (distanceToGoal() - 50) / 20.0;
        }
        
        // 确保分数在0-10范围内
        if (score < 0) score = 0;
        if (score > 10) score = 10;
        
        return score;
    }

    /**
     * @brief 查找最佳射门角度 (相对于守门员位置)
     * @param shooter_pos 射手位置
     * @return 最佳射门角度 (弧度)
     */
    double findBestShootingAngle(const point2f& shooter_pos) const {
        point2f goalie_pos = getPosition();
        point2f goal_center = getGoalCenter();
        
        // 默认射向球门中心
        double best_angle = (goal_center - shooter_pos).angle();
        
        // 如果守门员位置靠近球门中心，尝试射向球门角落
        if ((goalie_pos - goal_center).length() < 20) {
            // 上方球门角落
            point2f top_corner(goal_center.x, goal_center.y + GOAL_WIDTH_H);
            // 下方球门角落
            point2f bottom_corner(goal_center.x, goal_center.y - GOAL_WIDTH_H);
            
            // 计算到两个角落的距离
            double dist_to_top = (top_corner - shooter_pos).length();
            double dist_to_bottom = (bottom_corner - shooter_pos).length();
            
            // 选择较近的角落作为射门目标
            if (dist_to_top < dist_to_bottom) {
                best_angle = (top_corner - shooter_pos).angle();
            } else {
                best_angle = (bottom_corner - shooter_pos).angle();
            }
        } else {
            // 守门员不在中心位置，射向守门员对面的一侧
            if (goalie_pos.y > goal_center.y) {
                // 守门员在上方，射向下方
                point2f target(goal_center.x, goal_center.y - GOAL_WIDTH_H * 0.7);
                best_angle = (target - shooter_pos).angle();
            } else {
                // 守门员在下方，射向上方
                point2f target(goal_center.x, goal_center.y + GOAL_WIDTH_H * 0.7);
                best_angle = (target - shooter_pos).angle();
            }
        }
        
        return best_angle;
    }

    /**
     * @brief 检查敌方守门员是否完全在球门线上
     * @return 是否在球门线上
     */
    bool isOnGoalLine() const {
        point2f goalie_pos = getPosition();
        return fabs(goalie_pos.x - FIELD_LENGTH_H) < 10;
    }

    /**
     * @brief 检查敌方守门员的射门防守能力
     * @param shooter_pos 射手位置
     * @return 防守能力评分 (0-10，越高越难射门)
     */
    double evaluateShootingDifficulty(const point2f& shooter_pos) const {
        point2f goalie_pos = getPosition();
        point2f goal_center = getGoalCenter();
        
        // 计算射手到球门的距离
        double dist_to_goal = (shooter_pos - goal_center).length();
        
        // 计算射手到球门的方向
        point2f shoot_vector = goal_center - shooter_pos;
        double shoot_angle = shoot_vector.angle();
        
        // 计算守门员相对于射门路线的位置
        point2f goalie_rel_pos = goalie_pos - shooter_pos;
        double goalie_angle = goalie_rel_pos.angle();
        double angle_diff = fabs(anglemod(shoot_angle - goalie_angle));
        
        // 计算射手与守门员的距离
        double dist_to_goalie = goalie_rel_pos.length();
        
        // 计算射门难度评分
        double difficulty = 5.0;  // 基础难度
        
        // 如果守门员在射门路线上，增加难度
        if (angle_diff < 0.2) {
            difficulty += 3.0;
        } else if (angle_diff < 0.5) {
            difficulty += 2.0;
        }
        
        // 如果射手距离球门很近，降低难度
        if (dist_to_goal < 150) {
            difficulty -= 2.0;
        } else if (dist_to_goal < 250) {
            difficulty -= 1.0;
        }
        
        // 确保难度在0-10范围内
        if (difficulty < 0) difficulty = 0;
        if (difficulty > 10) difficulty = 10;
        
        return difficulty;
    }

private:
    const WorldModel* world_model;
    BallTools ball_tools;
};

#endif // OPP_GOALIE_H