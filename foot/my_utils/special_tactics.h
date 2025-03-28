#ifndef SPECIAL_TACTICS_H
#define SPECIAL_TACTICS_H

#include "tactics.h"
#include "../utils/PlayerTask.h"
#include "../utils/WorldModel.h"
#include "../utils/game_state.h"
#include "../utils/constants.h"
#include <memory>
#include <cmath>

// 游戏状态常量（从game_state.h中提取关键常量）
// 这些常量用于比赛状态判断
#define PM_STOP 0
#define PM_OurKickOff 1
#define PM_OppKickOff 2
#define PM_OurThrowIn 3
#define PM_OppThrowIn 4
#define PM_OurGoalKick 5
#define PM_OppGoalKick 6
#define PM_OurCornerKick 7
#define PM_OppCornerKick 8
#define PM_OurFreeKick 9
#define PM_OppFreeKick 10
#define PM_OurPenaltyKick 11
#define PM_OppPenaltyKick 12
#define PM_Normal 99

/**
 * @brief 开球战术类
 * 处理比赛开球情况的战术
 */
class KickoffTactic : public Tactic {
private:
    std::string name;
    TacticType type;

public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    explicit KickoffTactic(const WorldModel* model) : Tactic(model) {
        name = "Kickoff Tactic";
        type = TacticType::SPECIAL_SITUATION;
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
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        // 如果当前是开球状态，则评分高
        if (play_mode == PM_OurKickOff) {
            eval.score = 1.0;
            eval.description = "Our kickoff situation";
        } else if (play_mode == PM_OppKickOff) {
            eval.score = 0.8;
            eval.description = "Opponent kickoff situation";
        } else {
            eval.score = 0.0;
            eval.description = "Not a kickoff situation";
        }
        
        return eval;
    }

    /**
     * @brief 执行开球战术
     * @param robot_id 执行战术的机器人ID
     * @return 机器人任务
     */
    PlayerTask execute(int robot_id) override {
        PlayerTask task;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        if (play_mode == PM_OurKickOff) {
            // 我方开球
            
            // 在实际比赛中，会有专门指定的开球球员
            // 这里简单地判断：如果是最接近球的球员，则执行开球
            int closest_id = our_players->getClosestPlayerToBall();
            
            if (robot_id == closest_id) {
                // 开球机器人移动到球位置
                task.target_pos = ball_pos;
                
                // 面向对方球门
                task.orientate = atan2(0 - ball_pos.y, FIELD_LENGTH_H - ball_pos.x);
                
                // 如果接近球，执行踢球
                point2f robot_pos = world_model->get_our_player_pos(robot_id);
                if ((robot_pos - ball_pos).length() < 30) {
                    // 向前踢球
                    task.needKick = true;
                    task.kickPower = 3.0;
                }
            } else {
                // 非开球机器人散开站位
                float angle = (robot_id * 60) * M_PI / 180.0;  // 每个机器人间隔60度
                point2f position;
                position.x = -50 * cos(angle);  // 站在中心后方50厘米
                position.y = -50 * sin(angle);
                
                task.target_pos = position;
                task.orientate = atan2(0 - position.y, FIELD_LENGTH_H - position.x);
            }
        } else if (play_mode == PM_OppKickOff) {
            // 对方开球，保持在我方半场
            
            // 设置防守阵型
            float angle = (robot_id * 45) * M_PI / 180.0;  // 每个机器人间隔45度
            point2f position;
            
            // 站在中心圈外围
            position.x = -(120 + robot_id * 30) * cos(angle);
            position.y = -(120 + robot_id * 30) * sin(angle);
            
            task.target_pos = position;
            task.orientate = atan2(ball_pos.y - position.y, ball_pos.x - position.x);
        }
        
        return task;
    }
};

/**
 * @brief 任意球战术类
 * 处理比赛中的任意球情况
 */
class FreeKickTactic : public Tactic {
private:
    std::string name;
    TacticType type;

public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    explicit FreeKickTactic(const WorldModel* model) : Tactic(model) {
        name = "Free Kick Tactic";
        type = TacticType::SPECIAL_SITUATION;
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
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        // 如果当前是我方任意球状态，则评分高
        if (play_mode == PM_OurFreeKick) {
            eval.score = 1.0;
            eval.description = "Our free kick situation";
        } else if (play_mode == PM_OppFreeKick) {
            eval.score = 0.8;
            eval.description = "Opponent free kick situation";
        } else {
            eval.score = 0.0;
            eval.description = "Not a free kick situation";
        }
        
        return eval;
    }

    /**
     * @brief 执行任意球战术
     * @param robot_id 执行战术的机器人ID
     * @return 机器人任务
     */
    PlayerTask execute(int robot_id) override {
        PlayerTask task;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        if (play_mode == PM_OurFreeKick) {
            // 我方任意球
            
            // 获取对方球门位置
            point2f goal_pos(FIELD_LENGTH_H, 0);
            
            // 在实际比赛中，会有专门指定的任意球球员
            // 这里简单地判断：如果是最接近球的球员，则执行任意球
            int closest_id = our_players->getClosestPlayerToBall();
            
            if (robot_id == closest_id) {
                // 踢球机器人移动到球位置
                task.target_pos = ball_pos;
                
                // 计算到球门的方向
                task.orientate = atan2(goal_pos.y - ball_pos.y, goal_pos.x - ball_pos.x);
                
                // 如果接近球，检查是否有直接射门路径
                point2f robot_pos = world_model->get_our_player_pos(robot_id);
                if ((robot_pos - ball_pos).length() < 30) {
                    // 检查到球门的射门路径上是否有障碍物
                    bool path_clear = true;
                    
                    // 简单检查：如果距离球门较近且在对方半场，尝试直接射门
                    double dist_to_goal = (ball_pos - goal_pos).length();
                    if (ball_pos.x > 0 && dist_to_goal < 300) {
                        task.needKick = true;
                        task.kickPower = 8.0;  // 全力射门
                    } else {
                        // 寻找传球目标
                        int pass_target = -1;
                        double best_score = -1;
                        
                        // 评估所有队友的位置，找出最佳传球目标
                        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                            if (i == robot_id || !world_model->get_our_exist_id()[i]) continue;
                            
                            point2f teammate_pos = world_model->get_our_player_pos(i);
                            double dist_to_teammate = (ball_pos - teammate_pos).length();
                            double teammate_to_goal = (teammate_pos - goal_pos).length();
                            
                            // 简单评分：队友越靠近对方球门越好
                            double score = 1000 - teammate_to_goal;
                            
                            // 但距离不能太远
                            if (dist_to_teammate > 300) {
                                score *= 0.5;
                            }
                            
                            if (score > best_score) {
                                best_score = score;
                                pass_target = i;
                            }
                        }
                        
                        if (pass_target >= 0) {
                            // 有传球目标
                            point2f target_pos = world_model->get_our_player_pos(pass_target);
                            task.orientate = atan2(target_pos.y - ball_pos.y, target_pos.x - ball_pos.x);
                            task.needKick = true;
                            task.isPass = true;
                            task.kickPower = 3.0;  // 适中的传球力度
                        } else {
                            // 没有好的传球目标，尝试自己带球
                            task.needCb = true; // 开启吸球带球
                        }
                    }
                }
            } else {
                // 非踢球机器人寻找有利位置
                point2f strategic_pos;
                
                // 根据机器人ID分配不同位置
                if (robot_id % 3 == 0) {
                    // 一部分队员在前场中路
                    strategic_pos = point2f(ball_pos.x + 100, 0);
                } else if (robot_id % 3 == 1) {
                    // 一部分队员在左侧
                    strategic_pos = point2f(ball_pos.x + 70, -100);
                } else {
                    // 一部分队员在右侧
                    strategic_pos = point2f(ball_pos.x + 70, 100);
                }
                
                // 确保位置在场地范围内
                strategic_pos.x = std::min(strategic_pos.x, FIELD_LENGTH_H - 30.0);
                strategic_pos.y = std::min(std::max(strategic_pos.y, -FIELD_WIDTH_H + 30.0), FIELD_WIDTH_H - 30.0);
                
                task.target_pos = strategic_pos;
                task.orientate = atan2(goal_pos.y - strategic_pos.y, goal_pos.x - strategic_pos.x);
            }
        } else if (play_mode == PM_OppFreeKick) {
            // 对方任意球，需要防守
            
            // 计算所有机器人与球的距离
            double min_dist = 9999.0;
            int closest_id = -1;
            
            for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                if (world_model->get_our_exist_id()[i]) {
                    double dist = (world_model->get_our_player_pos(i) - ball_pos).length();
                    if (dist < min_dist) {
                        min_dist = dist;
                        closest_id = i;
                    }
                }
            }
            
            // 根据距离球的远近分配不同任务
            if (robot_id == closest_id) {
                // 最近的机器人尝试干扰对方射门，但保持规则要求的距离
                point2f our_goal(-FIELD_LENGTH_H, 0);
                point2f direction = (ball_pos - our_goal).normalize();
                point2f position = ball_pos - direction * 500;  // 规则要求的最小距离
                
                task.target_pos = position;
                task.orientate = atan2(ball_pos.y - position.y, ball_pos.x - position.x);
            } else if (robot_id % 5 == 1 || robot_id % 5 == 2) {
                // 第2、3近的机器人守卫球门两侧
                point2f goal_pos(-FIELD_LENGTH_H, 0);
                point2f position;
                
                if (robot_id % 5 == 1) {
                    // 守左侧
                    position = point2f(goal_pos.x + 50, -50);
                } else {
                    // 守右侧
                    position = point2f(goal_pos.x + 50, 50);
                }
                
                task.target_pos = position;
                task.orientate = atan2(ball_pos.y - position.y, ball_pos.x - position.x);
            } else {
                // 其余机器人布防拦截可能的传球路线
                point2f position;
                
                // 简单地在自己半场均匀分布
                float angle = ((robot_id % 3) * 45) * M_PI / 180.0;
                position.x = -200;
                position.y = 200 * sin(angle);
                
                task.target_pos = position;
                task.orientate = atan2(ball_pos.y - position.y, ball_pos.x - position.x);
            }
        }
        
        return task;
    }
};

/**
 * @brief 角球战术类
 * 处理比赛中的角球情况
 */
class CornerKickTactic : public Tactic {
private:
    std::string name;
    TacticType type;

public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    explicit CornerKickTactic(const WorldModel* model) : Tactic(model) {
        name = "Corner Kick Tactic";
        type = TacticType::SPECIAL_SITUATION;
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
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        // 如果当前是我方角球状态，则评分高
        if (play_mode == PM_OurCornerKick) {
            eval.score = 1.0;
            eval.description = "Our corner kick situation";
        } else if (play_mode == PM_OppCornerKick) {
            eval.score = 0.8;
            eval.description = "Opponent corner kick situation";
        } else {
            eval.score = 0.0;
            eval.description = "Not a corner kick situation";
        }
        
        return eval;
    }

    /**
     * @brief 执行角球战术
     * @param robot_id 执行战术的机器人ID
     * @return 机器人任务
     */
    PlayerTask execute(int robot_id) override {
        PlayerTask task;
        
        // 获取球位置
        point2f ball_pos = world_model->get_ball_pos();
        
        // 获取游戏状态
        int play_mode = world_model->get_game_state();
        
        if (play_mode == PM_OurCornerKick) {
            // 我方角球
            
            // 在实际比赛中，会有专门指定的角球球员
            // 这里简单地判断：如果是最接近球的球员，则执行角球
            int closest_id = our_players->getClosestPlayerToBall();
            
            if (robot_id == closest_id) {
                // 踢球机器人移动到球位置
                task.target_pos = ball_pos;
                
                // 判断是左侧还是右侧角球
                bool is_left_corner = (ball_pos.y < 0);
                
                // 针对角球位置寻找最佳传球目标
                int best_target = -1;
                point2f best_target_pos;
                
                // 找出禁区前沿的最佳队友
                point2f penalty_area_front(FIELD_LENGTH_H - 200, 0);
                double best_score = -1;
                
                for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
                    if (i == robot_id || !world_model->get_our_exist_id()[i]) continue;
                    
                    point2f teammate_pos = world_model->get_our_player_pos(i);
                    
                    // 计算到禁区前沿的距离
                    double dist_to_penalty = (teammate_pos - penalty_area_front).length();
                    
                    // 评分：距离禁区前沿越近越好
                    double score = 1000 - dist_to_penalty;
                    
                    // 确保传球路线畅通
                    bool path_clear = true; // 这里简化处理，实际应该检查传球路线上的障碍物
                    
                    if (path_clear && score > best_score) {
                        best_score = score;
                        best_target = i;
                        best_target_pos = teammate_pos;
                    }
                }
                
                point2f robot_pos = world_model->get_our_player_pos(robot_id);
                if ((robot_pos - ball_pos).length() < 30) {
                    // 接近球，准备传球或射门
                    if (best_target >= 0) {
                        // 有传球目标
                        task.orientate = atan2(best_target_pos.y - ball_pos.y, best_target_pos.x - ball_pos.x);
                        task.needKick = true;
                        task.isPass = true;
                        task.kickPower = 3.5;  // 较大力度传球
                    } else {
                        // 没有好的传球目标，尝试直接射门
                        point2f goal_pos(FIELD_LENGTH_H, 0);
                        task.orientate = atan2(goal_pos.y - ball_pos.y, goal_pos.x - ball_pos.x);
                        task.needKick = true;
                        task.kickPower = 8.0;  // 全力射门
                    }
                }
            } else {
                // 非踢球机器人寻找有利位置
                point2f strategic_pos;
                
                // 判断是左侧还是右侧角球
                bool is_left_corner = (ball_pos.y < 0);
                
                // 根据机器人ID分配不同位置
                if (robot_id % 4 == 0) {
                    // 第一个队员在禁区前中路
                    strategic_pos = point2f(FIELD_LENGTH_H - 150, 0);
                } else if (robot_id % 4 == 1) {
                    // 第二个队员在前点
                    strategic_pos = point2f(FIELD_LENGTH_H - 120, is_left_corner ? -50 : 50);
                } else if (robot_id % 4 == 2) {
                    // 第三个队员在后点
                    strategic_pos = point2f(FIELD_LENGTH_H - 120, is_left_corner ? 50 : -50);
                } else {
                    // 第四个队员在外围
                    strategic_pos = point2f(FIELD_LENGTH_H - 200, is_left_corner ? 100 : -100);
                }
                
                // 确保位置在场地范围内
                strategic_pos.x = std::min(strategic_pos.x, FIELD_LENGTH_H - 30.0);
                strategic_pos.y = std::min(std::max(strategic_pos.y, -FIELD_WIDTH_H + 30.0), FIELD_WIDTH_H - 30.0);
                
                task.target_pos = strategic_pos;
                task.orientate = atan2(ball_pos.y - strategic_pos.y, ball_pos.x - strategic_pos.x);
            }
        } else if (play_mode == PM_OppCornerKick) {
            // 对方角球，加强防守
            
            // 确定守门员ID
            int goalie_id = world_model->get_our_goalie();
            
            // 每个机器人分配不同的防守位置
            if (robot_id == goalie_id) {
                // 守门员
                task.target_pos = point2f(-FIELD_LENGTH_H + 10, 0);
                task.orientate = atan2(ball_pos.y - task.target_pos.y, ball_pos.x - task.target_pos.x);
            } else if (robot_id % 5 == 1) {
                // 第一个队员防守近门柱
                task.target_pos = point2f(-FIELD_LENGTH_H + 10, ball_pos.y > 0 ? 50 : -50);
                task.orientate = atan2(ball_pos.y - task.target_pos.y, ball_pos.x - task.target_pos.x);
            } else if (robot_id % 5 == 2) {
                // 第二个队员防守远门柱
                task.target_pos = point2f(-FIELD_LENGTH_H + 10, ball_pos.y > 0 ? -50 : 50);
                task.orientate = atan2(ball_pos.y - task.target_pos.y, ball_pos.x - task.target_pos.x);
            } else if (robot_id % 5 == 3) {
                // 第三个队员防守禁区前沿
                task.target_pos = point2f(-FIELD_LENGTH_H + 100, 0);
                task.orientate = atan2(ball_pos.y - task.target_pos.y, ball_pos.x - task.target_pos.x);
            } else {
                // 其余队员分布在防守边路
                int index = (robot_id % 5) - 3;
                float offset = index * 60;  // 间隔分布
                
                // 根据角球位置，在危险区域布防
                if (ball_pos.y > 0) {
                    // 右侧角球，防守右侧
                    task.target_pos = point2f(-FIELD_LENGTH_H/2 + index * 50, offset);
                } else {
                    // 左侧角球，防守左侧
                    task.target_pos = point2f(-FIELD_LENGTH_H/2 + index * 50, -offset);
                }
                
                task.orientate = atan2(ball_pos.y - task.target_pos.y, ball_pos.x - task.target_pos.x);
            }
        }
        
        return task;
    }
};

#endif // SPECIAL_TACTICS_H 