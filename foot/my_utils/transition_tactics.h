#ifndef TRANSITION_TACTICS_H
#define TRANSITION_TACTICS_H

#include "tactics.h"
#include "../utils/PlayerTask.h"
#include "../utils/WorldModel.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include <memory>
#include <cmath>

/**
 * @brief 快速反击战术类
 * 从防守转为进攻的快速反击战术
 */
class CounterAttackTactic : public Tactic {
private:
    std::string name;
    TacticType type;

public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    explicit CounterAttackTactic(const WorldModel* model) : Tactic(model) {
        name = "Counter Attack Tactic";
        type = TacticType::TRANSITION;
    }

    /**
     * @brief 获取战术名称
     * @return 战术名称
     */
    std::string getName() const override {
        return name;
    }

    /**
     * @brief 获取战术类型
     * @return 战术类型
     */
    TacticType getType() const override {
        return type;
    }

    /**
     * @brief 评估当前战术的适用性
     * @return 战术评估结果
     */
    TacticEvaluation evaluate() const override {
        TacticEvaluation eval;
        eval.score = 0.0;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取球速度
        point2f ball_vel = world_model->get_ball_vel();
        
        // 如果球在我方半场，且我方控球，且球正向前移动，则考虑快速反击
        if (ball_pos.x < 0 && ball_vel.x > 50) {
            // 检查我方是否控球
            bool our_ball_possession = false;
            for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                if (world_model->get_our_exist_id()[i]) {
                    point2f player_pos = world_model->get_our_player_pos(i);
                    
                    // 如果有球员靠近球且正在移动
                    if ((player_pos - ball_pos).length() < 30) {
                        our_ball_possession = true;
                        break;
                    }
                }
            }
            
            if (our_ball_possession) {
                // 检查前场是否开阔
                int opp_in_path = 0;
                
                // 计算通向对方球门的路径上有多少对手
                for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                    if (world_model->get_opp_exist_id()[i]) {
                        point2f opp_pos = world_model->get_opp_player_pos(i);
                        
                        // 简单判断：如果对手在球和对方球门之间，则路径不开阔
                        if (opp_pos.x > ball_pos.x && opp_pos.x < FIELD_LENGTH_H) {
                            opp_in_path++;
                        }
                    }
                }
                
                // 如果对手较少，则快速反击得分高
                if (opp_in_path < 3) {
                    eval.score = 0.8;
                    eval.description = "Good counter attack opportunity";
                } else {
                    eval.score = 0.4;
                    eval.description = "Limited counter attack opportunity";
                }
            }
        }
        
        return eval;
    }

    /**
     * @brief 执行快速反击战术
     * @param robot_id 执行战术的机器人ID
     * @return 机器人任务
     */
    PlayerTask execute(int robot_id) override {
        PlayerTask task;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取目标位置（对方球门）
        point2f goal_pos(FIELD_LENGTH_H, 0);
        
        // 获取当前球员位置
        point2f player_pos = world_model->get_our_player_pos(robot_id);
        
        // 判断球员是否为最接近球的队员
        int closest_id = our_players->getClosestPlayerToBall();
        bool is_closest = (robot_id == closest_id);
        
        if (is_closest) {
            // 最接近球的队员去控球
            if ((player_pos - ball_pos).length() < 30) {
                // 已经控球，向对方球门方向快速移动
                point2f target_pos = ball_pos;
                target_pos.x += 150;  // 向前移动150厘米
                
                task.target_pos = target_pos;
                task.orientate = atan2(goal_pos.y - ball_pos.y, goal_pos.x - ball_pos.x);
                task.needCb = true;   // 启用吸球
            } else {
                // 还未控球，追球
                task.target_pos = ball_pos;
                task.orientate = atan2(goal_pos.y - ball_pos.y, goal_pos.x - ball_pos.x);
            }
        } else {
            // 非控球队员快速前插
            point2f forward_pos;
            
            // 根据ID选择不同的前插路线
            if (robot_id % 3 == 0) {
                // 中路前插
                forward_pos = point2f(ball_pos.x + 200, 0);
            } else if (robot_id % 3 == 1) {
                // 左路前插
                forward_pos = point2f(ball_pos.x + 150, -150);
            } else {
                // 右路前插
                forward_pos = point2f(ball_pos.x + 150, 150);
            }
            
            // 确保位置在场地范围内
            if (forward_pos.x > FIELD_LENGTH_H - 30.0) {
                forward_pos.x = FIELD_LENGTH_H - 30.0;
            }
            if (forward_pos.y < -FIELD_WIDTH_H + 30.0) {
                forward_pos.y = -FIELD_WIDTH_H + 30.0;
            } else if (forward_pos.y > FIELD_WIDTH_H - 30.0) {
                forward_pos.y = FIELD_WIDTH_H - 30.0;
            }
            
            task.target_pos = forward_pos;
            task.orientate = atan2(goal_pos.y - forward_pos.y, goal_pos.x - forward_pos.x);
        }
        
        return task;
    }
};

/**
 * @brief 快速回防战术类
 * 从进攻转为防守的快速回防战术
 */
class QuickDefenseTactic : public Tactic {
private:
    std::string name;
    TacticType type;

public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    explicit QuickDefenseTactic(const WorldModel* model) : Tactic(model) {
        name = "Quick Defense Tactic";
        type = TacticType::TRANSITION;
    }

    /**
     * @brief 获取战术名称
     * @return 战术名称
     */
    std::string getName() const override {
        return name;
    }

    /**
     * @brief 获取战术类型
     * @return 战术类型
     */
    TacticType getType() const override {
        return type;
    }

    /**
     * @brief 评估当前战术的适用性
     * @return 战术评估结果
     */
    TacticEvaluation evaluate() const override {
        TacticEvaluation eval;
        eval.score = 0.0;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取球速度
        point2f ball_vel = world_model->get_ball_vel();
        
        // 如果球在对方半场，但正向我方半场移动，则考虑快速回防
        if (ball_pos.x > 0 && ball_vel.x < -50) {
            // 检查对方是否控球
            bool opp_ball_possession = false;
            for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                if (world_model->get_opp_exist_id()[i]) {
                    point2f opp_pos = world_model->get_opp_player_pos(i);
                    
                    // 如果有对手靠近球且正在移动
                    if ((opp_pos - ball_pos).length() < 30) {
                        opp_ball_possession = true;
                        break;
                    }
                }
            }
            
            // 如果对方控球
            if (opp_ball_possession) {
                // 检查我方半场是否有足够防守队员
                int defenders_count = 0;
                for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                    if (world_model->get_our_exist_id()[i]) {
                        point2f player_pos = world_model->get_our_player_pos(i);
                        
                        // 如果队员在我方半场
                        if (player_pos.x < 0) {
                            defenders_count++;
                        }
                    }
                }
                
                // 如果我方半场防守队员不足，则快速回防得分高
                if (defenders_count < 3) {
                    eval.score = 0.9;
                    eval.description = "Urgent defensive transition needed";
                } else {
                    eval.score = 0.5;
                    eval.description = "Defensive transition may be beneficial";
                }
            }
        }
        
        return eval;
    }

    /**
     * @brief 执行快速回防战术
     * @param robot_id 执行战术的机器人ID
     * @return 机器人任务
     */
    PlayerTask execute(int robot_id) override {
        PlayerTask task;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取球速度
        point2f ball_vel = world_model->get_ball_vel();
        
        // 预测球的运动轨迹
        point2f predicted_ball_pos = ball_pos;
        if (ball_vel.length() > 10) {
            // 简单预测：球再移动2秒钟的位置
            predicted_ball_pos.x += ball_vel.x * 2;
            predicted_ball_pos.y += ball_vel.y * 2;
            
            // 确保预测位置在场地范围内
            if (predicted_ball_pos.x < -FIELD_LENGTH_H) {
                predicted_ball_pos.x = -FIELD_LENGTH_H;
            } else if (predicted_ball_pos.x > FIELD_LENGTH_H) {
                predicted_ball_pos.x = FIELD_LENGTH_H;
            }
            
            if (predicted_ball_pos.y < -FIELD_WIDTH_H) {
                predicted_ball_pos.y = -FIELD_WIDTH_H;
            } else if (predicted_ball_pos.y > FIELD_WIDTH_H) {
                predicted_ball_pos.y = FIELD_WIDTH_H;
            }
        }
        
        // 我方球门位置
        point2f our_goal(-FIELD_LENGTH_H, 0);
        
        // 基于球员ID分配不同的防守位置
        point2f defense_pos;
        
        // 获取守门员ID
        int goalie_id = world_model->get_our_goalie();
        
        if (robot_id == goalie_id) {
            // 如果是守门员，直接回到球门
            defense_pos = point2f(-FIELD_LENGTH_H + 20, 0);
        } else if (robot_id % 5 == 1) {
            // 第一个队员封堵传球路线
            defense_pos.x = (predicted_ball_pos.x + our_goal.x) / 2;
            defense_pos.y = (predicted_ball_pos.y + our_goal.y) / 2;
        } else if (robot_id % 5 == 2) {
            // 第二个队员直接去拦截球
            defense_pos = predicted_ball_pos;
        } else if (robot_id % 5 == 3) {
            // 第三个队员守护左侧
            defense_pos = point2f(-FIELD_LENGTH_H / 2, -FIELD_WIDTH_H / 3);
        } else if (robot_id % 5 == 4) {
            // 第四个队员守护右侧
            defense_pos = point2f(-FIELD_LENGTH_H / 2, FIELD_WIDTH_H / 3);
        } else {
            // 其余队员守护中路
            defense_pos = point2f(-FIELD_LENGTH_H / 2, 0);
        }
        
        // 确保位置在场地范围内
        if (defense_pos.x < -FIELD_LENGTH_H + 30) {
            defense_pos.x = -FIELD_LENGTH_H + 30;
        } else if (defense_pos.x > FIELD_LENGTH_H - 30) {
            defense_pos.x = FIELD_LENGTH_H - 30;
        }
        
        if (defense_pos.y < -FIELD_WIDTH_H + 30) {
            defense_pos.y = -FIELD_WIDTH_H + 30;
        } else if (defense_pos.y > FIELD_WIDTH_H - 30) {
            defense_pos.y = FIELD_WIDTH_H - 30;
        }
        
        // 设置任务
        task.target_pos = defense_pos;
        task.orientate = atan2(ball_pos.y - defense_pos.y, ball_pos.x - defense_pos.x);
        
        return task;
    }
};

#endif // TRANSITION_TACTICS_H 