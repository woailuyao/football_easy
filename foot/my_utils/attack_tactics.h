#ifndef ATTACK_TACTICS_H
#define ATTACK_TACTICS_H

#include "tactics.h"
#include "communication.h"

/**
 * @brief 直接进攻战术
 */
class DirectAttackTactic : public Tactic {
public:
    DirectAttackTactic(const WorldModel* model) : Tactic(model) {}
    
    std::string getName() const override {
        return "DirectAttack";
    }
    
    TacticType getType() const override {
        return TacticType::ATTACK;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Direct attack evaluation: ";
        
        // 评估条件1: 球在对方半场
        if (ball_tools->isInOpponentHalf()) {
            score += 3.0;
            desc += "Ball in opponent half (+3.0); ";
        }
        
        // 评估条件2: 我方球员接近球
        int closest_id = our_players->getClosestPlayerToBall();
        if (closest_id >= 0) {
            double dist = our_players->distanceToBall(closest_id);
            if (dist < 50) {
                score += 2.0;
                desc += "Player close to ball (+2.0); ";
            } else if (dist < 100) {
                score += 1.0;
                desc += "Player relatively close to ball (+1.0); ";
            }
        }
        
        // 评估条件3: 到对方球门的距离
        point2f ball_pos = ball_tools->getPosition();
        point2f goal_pos(FIELD_LENGTH_H, 0);
        double dist_to_goal = (ball_pos - goal_pos).length();
        
        if (dist_to_goal < 200) {
            score += 3.0;
            desc += "Close to opponent goal (+3.0); ";
        } else if (dist_to_goal < 400) {
            score += 1.5;
            desc += "Medium distance to opponent goal (+1.5); ";
        }
        
        // 评估条件4: 对方守门员位置
        double goalie_score = opp_goalie->evaluateDefensivePosition();
        if (goalie_score < 5.0) {
            score += 2.0;
            desc += "Opponent goalie in weak position (+2.0); ";
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        // 球的当前位置
        point2f ball_pos = ball_tools->getPosition();
        point2f player_pos = our_players->getPosition(robot_id);
        
        // 对方球门位置
        point2f goal_pos(FIELD_LENGTH_H, 0);
        
        // 选择最佳射门角度
        double shoot_angle = opp_goalie->findBestShootingAngle(player_pos);
        
        // 检查是否持球或接近球
        if (our_players->canHoldBall(robot_id)) {
            // 已经持球，创建射门任务
            point2f opp_goalie_pos = opp_goalie->getPosition();
            
            // 检查是否有好的射门机会
            double shoot_difficulty = opp_goalie->evaluateShootingDifficulty(player_pos);
            
            if (shoot_difficulty < 6.0) {
                // 较好的射门机会，直接射门
                return our_players->createShootTask(robot_id);
            } else {
                // 射门难度较大，尝试靠近球门或寻找更好的射门角度
                point2f target_pos = player_pos + point2f(30, 0);  // 向前移动
                target_pos.x = std::min(target_pos.x, FIELD_LENGTH_H - 50.0);  // 不要太靠近边界
                
                // 避免对方球员
                int closest_opp = opp_players->getClosestPlayerToPosition(player_pos);
                if (closest_opp >= 0) {
                    point2f opp_pos = opp_players->getPosition(closest_opp);
                    point2f avoid_dir = player_pos - opp_pos;
                    if (avoid_dir.length() < 40) {
                        avoid_dir = avoid_dir / avoid_dir.length() * 20;
                        target_pos = target_pos + avoid_dir;
                    }
                }
                
                return our_players->createDribbleTask(robot_id, target_pos);
            }
        } else {
            // 未持球，移动到球的位置
            return our_players->createMoveTask(robot_id, ball_pos, (goal_pos - ball_pos).angle());
        }
    }
};

/**
 * @brief 传球射门战术
 */
class PassAndShootTactic : public Tactic {
public:
    PassAndShootTactic(const WorldModel* model) : Tactic(model) {}
    
    std::string getName() const override {
        return "PassAndShoot";
    }
    
    TacticType getType() const override {
        return TacticType::ATTACK;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Pass and shoot evaluation: ";
        
        // 评估条件1: 球在对方半场
        if (ball_tools->isInOpponentHalf()) {
            score += 2.0;
            desc += "Ball in opponent half (+2.0); ";
        }
        
        // 评估条件2: 有两个球员在对方半场
        int players_in_opponent_half = 0;
        std::vector<int> player_ids = our_players->getPlayerIds();
        for (int id : player_ids) {
            if (our_players->isInOpponentHalf(id)) {
                players_in_opponent_half++;
            }
        }
        
        if (players_in_opponent_half >= 2) {
            score += 3.0;
            desc += "Multiple players in opponent half (+3.0); ";
        } else if (players_in_opponent_half == 1) {
            score += 1.0;
            desc += "One player in opponent half (+1.0); ";
        }
        
        // 评估条件3: 球员之间有良好的传球路线
        int closest_id = our_players->getClosestPlayerToBall();
        if (closest_id >= 0) {
            for (int id : player_ids) {
                if (id != closest_id) {
                    point2f pos1 = our_players->getPosition(closest_id);
                    point2f pos2 = our_players->getPosition(id);
                    
                    // 检查传球路线上是否有对方球员
                    // 简化实现，实际中需要更复杂的碰撞检测
                    bool clear_path = true;
                    std::vector<int> opp_ids = opp_players->getPlayerIds();
                    for (int opp_id : opp_ids) {
                        point2f opp_pos = opp_players->getPosition(opp_id);
                        
                        // 检查对方球员是否在传球路线附近
                        double dist = distanceToLine(opp_pos, pos1, pos2);
                        if (dist < 20) {
                            clear_path = false;
                            break;
                        }
                    }
                    
                    if (clear_path) {
                        score += 2.5;
                        desc += "Clear passing lane available (+2.5); ";
                        break;
                    }
                }
            }
        }
        
        // 评估条件4: 对方防守态势
        int threats = opp_players->getThreatPlayers().size();
        if (threats < 2) {
            score += 1.5;
            desc += "Few defensive threats (+1.5); ";
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        std::vector<int> player_ids = our_players->getPlayerIds();
        
        // 获取最接近球的我方球员
        int closest_to_ball = our_players->getClosestPlayerToBall();
        point2f ball_pos = ball_tools->getPosition();
        
        // 对方球门位置
        point2f goal_pos(FIELD_LENGTH_H, 0);
        
        // 判断当前球员是否是最接近球的球员
        if (robot_id == closest_to_ball) {
            // 如果是接近球的球员，判断是否持球
            if (our_players->canHoldBall(robot_id)) {
                // 寻找传球目标
                int pass_target = -1;
                double best_score = -1;
                
                for (int id : player_ids) {
                    if (id != robot_id) {
                        point2f target_pos = our_players->getPosition(id);
                        
                        // 计算传球评分
                        double score = 0;
                        
                        // 加分: 目标球员接近对方球门
                        double dist_to_goal = (target_pos - goal_pos).length();
                        score += (800 - std::min(dist_to_goal, 800.0)) / 100;
                        
                        // 加分: 有清晰的传球路线
                        bool clear_path = true;
                        point2f player_pos = our_players->getPosition(robot_id);
                        std::vector<int> opp_ids = opp_players->getPlayerIds();
                        for (int opp_id : opp_ids) {
                            point2f opp_pos = opp_players->getPosition(opp_id);
                            double dist = distanceToLine(opp_pos, player_pos, target_pos);
                            if (dist < 20) {
                                clear_path = false;
                                break;
                            }
                        }
                        
                        if (clear_path) {
                            score += 3;
                        }
                        
                        // 加分: 球员在对方半场
                        if (our_players->isInOpponentHalf(id)) {
                            score += 2;
                        }
                        
                        // 更新最佳传球目标
                        if (score > best_score) {
                            best_score = score;
                            pass_target = id;
                        }
                    }
                }
                
                // 如果找到了好的传球目标
                if (pass_target >= 0 && best_score > 3) {
                    // 发送传球意图通信
                    // Communication::getInstance().sendPassIntention(pass_target, our_players->getPosition(pass_target));
                    
                    // 执行传球
                    return our_players->createPassTask(robot_id, pass_target);
                } else {
                    // 没有好的传球目标，尝试自己带球或射门
                    double shoot_difficulty = opp_goalie->evaluateShootingDifficulty(our_players->getPosition(robot_id));
                    if (shoot_difficulty < 7) {
                        return our_players->createShootTask(robot_id);
                    } else {
                        point2f dribble_target = our_players->getPosition(robot_id) + point2f(40, 0);
                        dribble_target.x = std::min(dribble_target.x, FIELD_LENGTH_H - 50.0);
                        
                        return our_players->createDribbleTask(robot_id, dribble_target);
                    }
                }
            } else {
                // 未持球，移动到球的位置
                return our_players->createMoveTask(robot_id, ball_pos);
            }
        } else {
            // 不是最接近球的球员，寻找合适的位置等待传球
            
            // 判断当前是否有传球意图针对该球员
            // Message pass_msg = Communication::getInstance().receiveMessage(MessageType::PASS_INTENTION);
            // if (pass_msg.receiver_id == robot_id) {
            //     // 有传球意图，移动到传球目标位置
            //     return our_players->createMoveTask(robot_id, pass_msg.position, 
            //                                    (ball_pos - pass_msg.position).angle());
            // }
            
            // 没有传球意图，寻找进攻位置
            // 在对方半场找一个有利的进攻位置
            point2f attack_pos;
            
            // 根据球员序号选择不同的位置策略
            if (robot_id % 2 == 0) {
                // 一个球员在中路偏右
                attack_pos = point2f(FIELD_LENGTH_H - 150, 100);
            } else {
                // 一个球员在中路偏左
                attack_pos = point2f(FIELD_LENGTH_H - 150, -100);
            }
            
            // 避免与其他球员位置过近
            for (int id : player_ids) {
                if (id != robot_id) {
                    point2f other_pos = our_players->getPosition(id);
                    if ((attack_pos - other_pos).length() < 80) {
                        // 调整位置，增加间距
                        point2f offset = attack_pos - other_pos;
                        offset = offset / offset.length() * 40;
                        attack_pos = attack_pos + offset;
                    }
                }
            }
            
            // 确保位置在场地范围内
            attack_pos.x = std::min(std::max(attack_pos.x, -FIELD_LENGTH_H), FIELD_LENGTH_H - 20);
            attack_pos.y = std::min(std::max(attack_pos.y, -FIELD_WIDTH_H), FIELD_WIDTH_H);
            
            return our_players->createMoveTask(robot_id, attack_pos, (ball_pos - attack_pos).angle());
        }
    }
    
private:
    /**
     * @brief 计算点到线段的距离
     * @param p 点
     * @param a 线段起点
     * @param b 线段终点
     * @return 距离
     */
    double distanceToLine(const point2f& p, const point2f& a, const point2f& b) const {
        double length = (b - a).length();
        if (length < 0.0001) {
            return (p - a).length();
        }
        
        // 计算投影
        double t = ((p - a) * (b - a)) / (length * length);
        t = std::max(0.0, std::min(1.0, t));  // 限制在线段范围内
        
        point2f projection = a + (b - a) * t;
        return (p - projection).length();
    }
};

/**
 * @brief 边路进攻战术
 */
class WingAttackTactic : public Tactic {
public:
    WingAttackTactic(const WorldModel* model) : Tactic(model) {}
    
    std::string getName() const override {
        return "WingAttack";
    }
    
    TacticType getType() const override {
        return TacticType::ATTACK;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Wing attack evaluation: ";
        
        // 评估条件1: 球在边路
        point2f ball_pos = ball_tools->getPosition();
        if (fabs(ball_pos.y) > FIELD_WIDTH_H/2) {
            score += 3.0;
            desc += "Ball on the wing (+3.0); ";
        }
        
        // 评估条件2: 前场有球员
        std::vector<int> player_ids = our_players->getPlayerIds();
        bool forward_player = false;
        for (int id : player_ids) {
            point2f pos = our_players->getPosition(id);
            if (pos.x > 0 && pos.x < FIELD_LENGTH_H - 100) {
                forward_player = true;
                break;
            }
        }
        
        if (forward_player) {
            score += 2.0;
            desc += "Forward player available (+2.0); ";
        }
        
        // 评估条件3: 中路对方防守密集
        int center_defenders = 0;
        std::vector<int> opp_ids = opp_players->getPlayerIds();
        for (int id : opp_ids) {
            point2f pos = opp_players->getPosition(id);
            if (pos.x > 0 && fabs(pos.y) < 100) {
                center_defenders++;
            }
        }
        
        if (center_defenders >= 2) {
            score += 2.0;
            desc += "Center heavily defended (+2.0); ";
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        std::vector<int> player_ids = our_players->getPlayerIds();
        point2f ball_pos = ball_tools->getPosition();
        point2f player_pos = our_players->getPosition(robot_id);
        
        // 获取最接近球的我方球员
        int closest_to_ball = our_players->getClosestPlayerToBall();
        
        // 判断当前球员是否是最接近球的球员
        if (robot_id == closest_to_ball) {
            // 最接近球的球员负责控球和传中
            if (our_players->canHoldBall(robot_id)) {
                // 已持球，判断是否进行传中或者继续带球
                
                // 计算到球门的距离
                point2f goal_center(FIELD_LENGTH_H, 0);
                double dist_to_goal = (player_pos - goal_center).length();
                
                // 如果已经在禁区前沿或靠近球门
                if (player_pos.x > FIELD_LENGTH_H - 100 && fabs(player_pos.y) < 100) {
                    // 尝试射门
                    return our_players->createShootTask(robot_id);
                } else if (player_pos.x > FIELD_LENGTH_H - 200) {
                    // 足够靠近，尝试传中
                    
                    // 寻找禁区内或前沿的接应队友
                    int best_target = -1;
                    double best_score = -1;
                    
                    for (int id : player_ids) {
                        if (id != robot_id) {
                            point2f target_pos = our_players->getPosition(id);
                            
                            // 计算传球得分
                            double score = 0;
                            
                            // 目标球员在禁区内或前沿
                            if (target_pos.x > FIELD_LENGTH_H - 150 && fabs(target_pos.y) < 110) {
                                score += 5;
                                
                                // 加分: 离球门更近
                                score += (200 - std::min((target_pos - goal_center).length(), 200.0)) / 40;
                                
                                // 加分: 没有被紧密盯防
                                bool tightly_marked = false;
                                for (int opp_id : opp_players->getPlayerIds()) {
                                    if ((opp_players->getPosition(opp_id) - target_pos).length() < 30) {
                                        tightly_marked = true;
                                        break;
                                    }
                                }
                                
                                if (!tightly_marked) {
                                    score += 3;
                                }
                                
                                // 更新最佳目标
                                if (score > best_score) {
                                    best_score = score;
                                    best_target = id;
                                }
                            }
                        }
                    }
                    
                    if (best_target >= 0) {
                        // 发送传球意图
                        // Communication::getInstance().sendPassIntention(best_target, our_players->getPosition(best_target));
                        
                        // 执行传中
                        return our_players->createPassTask(robot_id, best_target, 4.0);  // 传球力量稍大
                    } else {
                        // 如果没有合适的接应队友，直接带球到传中位置
                        point2f dribble_target(FIELD_LENGTH_H - 130, player_pos.y > 0 ? 100 : -100);
                        return our_players->createDribbleTask(robot_id, dribble_target);
                    }
                } else {
                    // 还不够靠近，继续带球推进
                    double wing_y = player_pos.y > 0 ? FIELD_WIDTH_H * 0.7 : -FIELD_WIDTH_H * 0.7;
                    point2f dribble_target(player_pos.x + 80, wing_y);
                    
                    // 确保目标在场地范围内
                    dribble_target.x = std::min(dribble_target.x, FIELD_LENGTH_H - 50);
                    
                    return our_players->createDribbleTask(robot_id, dribble_target);
                }
            } else {
                // 未持球，移动到球的位置
                return our_players->createMoveTask(robot_id, ball_pos);
            }
        } else {
            // 不是最接近球的球员，负责寻找接应位置
            
            // 判断当前是否有传球意图针对该球员
            // Message pass_msg = Communication::getInstance().receiveMessage(MessageType::PASS_INTENTION);
            // if (pass_msg.receiver_id == robot_id) {
            //     // 有传球意图，移动到传球目标位置
            //     return our_players->createMoveTask(robot_id, pass_msg.position, 
            //                                    (ball_pos - pass_msg.position).angle());
            // }
            
            // 根据当前位置和球的位置，找到适合的接应点
            point2f goal_center(FIELD_LENGTH_H, 0);
            point2f attack_pos;
            
            // 如果球在右侧，接应点在左侧禁区前沿，反之亦然
            if (ball_pos.y > 0) {
                attack_pos = point2f(FIELD_LENGTH_H - 100, -70);
            } else {
                attack_pos = point2f(FIELD_LENGTH_H - 100, 70);
            }
            
            // 避免与其他球员位置过近
            for (int id : player_ids) {
                if (id != robot_id) {
                    point2f other_pos = our_players->getPosition(id);
                    if ((attack_pos - other_pos).length() < 60) {
                        // 调整位置，增加间距
                        attack_pos.y = attack_pos.y > 0 ? attack_pos.y + 40 : attack_pos.y - 40;
                    }
                }
            }
            
            // 确保位置在场地范围内
            attack_pos.x = std::min(std::max(attack_pos.x, -FIELD_LENGTH_H), FIELD_LENGTH_H - 20);
            attack_pos.y = std::min(std::max(attack_pos.y, -FIELD_WIDTH_H), FIELD_WIDTH_H);
            
            return our_players->createMoveTask(robot_id, attack_pos, (goal_center - attack_pos).angle());
        }
    }
};

#endif // ATTACK_TACTICS_H 