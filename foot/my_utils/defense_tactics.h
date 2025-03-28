#ifndef DEFENSE_TACTICS_H
#define DEFENSE_TACTICS_H

#include "tactics.h"
#include "communication.h"

/**
 * @brief 人盯人防守战术
 */
class ManMarkingTactic : public Tactic {
public:
    ManMarkingTactic(const WorldModel* model) : Tactic(model), target_id(-1) {}
    
    std::string getName() const override {
        return "ManMarking";
    }
    
    TacticType getType() const override {
        return TacticType::DEFENSE;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Man marking evaluation: ";
        
        // 评估条件1: 球在我方半场
        if (ball_tools->isInOurHalf()) {
            score += 2.0;
            desc += "Ball in our half (+2.0); ";
        }
        
        // 评估条件2: 敌方有球员接近我方禁区
        std::vector<int> threats = opp_players->getThreatPlayers();
        if (!threats.empty()) {
            score += 3.0;
            desc += "Opponent threats detected (+3.0); ";
        }
        
        // 评估条件3: 敌方球员数量少，适合人盯人
        if (threats.size() <= 2) {
            score += 2.0;
            desc += "Few opponents to mark (+2.0); ";
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        // 获取敌方威胁球员
        std::vector<int> threats = opp_players->getThreatPlayers();
        
        // 如果没有威胁，找到最靠近我方球门的敌人
        if (threats.empty()) {
            int closest_to_goal = -1;
            double min_dist = 9999.0;
            
            for (int id : opp_players->getPlayerIds()) {
                double dist = opp_players->distanceToOurGoal(id);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest_to_goal = id;
                }
            }
            
            if (closest_to_goal >= 0) {
                threats.push_back(closest_to_goal);
            }
        }
        
        // 仍然没有找到威胁，回到一个默认防守位置
        if (threats.empty()) {
            point2f defense_pos(-200, 0);
            return our_players->createMoveTask(robot_id, defense_pos);
        }
        
        // 计算要标记的敌人ID
        int mark_id = threats[0];  // 默认标记第一个威胁
        
        // 获取要标记的敌人位置
        point2f mark_pos = opp_players->getPosition(mark_id);
        
        // 我方球门位置
        point2f goal_pos(-FIELD_LENGTH_H, 0);
        
        // 计算防守位置 - 位于球门和敌人之间
        point2f goal_to_opp = mark_pos - goal_pos;
        double dist = goal_to_opp.length();
        
        // 防守位置在敌人和球门连线上，靠近敌人
        point2f defense_pos = mark_pos - goal_to_opp / dist * 30;
        
        // 如果球在敌人附近，更靠近球的位置防守
        point2f ball_pos = ball_tools->getPosition();
        if ((mark_pos - ball_pos).length() < 50) {
            point2f ball_to_opp = mark_pos - ball_pos;
            double ball_dist = ball_to_opp.length();
            
            if (ball_dist > 0.001) {
                // 位于球和敌人之间
                defense_pos = mark_pos - ball_to_opp / ball_dist * 20;
            }
        }
        
        // 确保防守位置在场地范围内
        defense_pos.x = std::min(std::max(defense_pos.x, -FIELD_LENGTH_H), FIELD_LENGTH_H);
        defense_pos.y = std::min(std::max(defense_pos.y, -FIELD_WIDTH_H), FIELD_WIDTH_H);
        
        // 设置防守任务
        PlayerTask task = our_players->createMoveTask(robot_id, defense_pos);
        
        // 如果球在防守球员附近且没有其他球员更靠近，尝试抢球
        if (our_players->distanceToBall(robot_id) < 50 && 
            robot_id == our_players->getClosestPlayerToBall()) {
            task = our_players->createMoveTask(robot_id, ball_pos);
        }
        
        return task;
    }
    
    /**
     * @brief 设置要标记的目标ID
     * @param id 敌方球员ID
     */
    void setTargetId(int id) {
        target_id = id;
    }
    
private:
    mutable int target_id;  // 要标记的敌方球员ID
};

/**
 * @brief 区域联防战术
 */
class ZoneDefenseTactic : public Tactic {
public:
    ZoneDefenseTactic(const WorldModel* model) : Tactic(model) {}
    
    std::string getName() const override {
        return "ZoneDefense";
    }
    
    TacticType getType() const override {
        return TacticType::DEFENSE;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Zone defense evaluation: ";
        
        // 评估条件1: 球在我方半场
        if (ball_tools->isInOurHalf()) {
            score += 2.0;
            desc += "Ball in our half (+2.0); ";
        }
        
        // 评估条件2: 敌方有多个球员在进攻
        std::vector<int> threats = opp_players->getThreatPlayers();
        if (threats.size() >= 2) {
            score += 3.0;
            desc += "Multiple opponent threats (+3.0); ";
        }
        
        // 评估条件3: 球靠近我方禁区
        if (ball_tools->isInPenaltyArea(true) || 
            (ball_tools->getPosition().x < -FIELD_LENGTH_H/2 && ball_tools->isInOurHalf())) {
            score += 2.5;
            desc += "Ball near our penalty area (+2.5); ";
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        point2f ball_pos = ball_tools->getPosition();
        point2f goal_pos(-FIELD_LENGTH_H, 0);
        
        // 计算球到球门的矢量
        point2f ball_to_goal = goal_pos - ball_pos;
        double dist = ball_to_goal.length();
        
        // 将球场划分为几个防守区域，并根据球员ID分配不同区域
        point2f defense_pos;
        
        // 获取所有球员ID，决定当前球员防守哪个区域
        std::vector<int> player_ids = our_players->getPlayerIds();
        int num_players = player_ids.size();
        
        // 找到当前球员在队伍中的索引
        int player_index = -1;
        for (int i = 0; i < num_players; i++) {
            if (player_ids[i] == robot_id) {
                player_index = i;
                break;
            }
        }
        
        // 根据球员索引分配不同的防守区域
        if (player_index == 0 || player_index == -1) {
            // 第一个球员或索引无效时防守中路
            defense_pos = point2f(-FIELD_LENGTH_H/2 + 100, 0);
        } else if (player_index == 1) {
            // 第二个球员防守左路
            defense_pos = point2f(-FIELD_LENGTH_H/2 + 100, -100);
        } else if (player_index == 2) {
            // 第三个球员防守右路
            defense_pos = point2f(-FIELD_LENGTH_H/2 + 100, 100);
        } else {
            // 其余球员随机分布
            int zone = player_index % 3;
            defense_pos = point2f(-FIELD_LENGTH_H/2 + 150, (zone - 1) * 100);
        }
        
        // 根据球的位置调整防守位置
        // 如果球靠近某个区域，相应区域防守球员向球移动
        point2f adjusted_pos = defense_pos;
        
        // 计算球的区域
        int ball_zone = 0;  // 中路
        if (ball_pos.y < -75) {
            ball_zone = -1;  // 左路
        } else if (ball_pos.y > 75) {
            ball_zone = 1;   // 右路
        }
        
        // 如果球员负责的区域与球所在区域相同，向球移动
        if ((player_index == 0 && ball_zone == 0) ||
            (player_index == 1 && ball_zone == -1) ||
            (player_index == 2 && ball_zone == 1)) {
            // 向球的方向靠近，但保持一定距离
            point2f to_ball = ball_pos - defense_pos;
            double ball_dist = to_ball.length();
            
            // 如果离球很远，向球移动一段距离
            if (ball_dist > 100) {
                to_ball = to_ball / ball_dist * 70;  // 向球方向移动70cm
                adjusted_pos = defense_pos + to_ball;
            }
        }
        
        // 确保防守位置在场地范围内
        adjusted_pos.x = std::min(std::max(adjusted_pos.x, -FIELD_LENGTH_H + 30), 0.0);
        adjusted_pos.y = std::min(std::max(adjusted_pos.y, -FIELD_WIDTH_H + 30), FIELD_WIDTH_H - 30);
        
        // 如果球员是最接近球的队员，考虑抢球
        if (robot_id == our_players->getClosestPlayerToBall() && 
            ball_pos.x < 0 && 
            (ball_pos - our_players->getPosition(robot_id)).length() < 100) {
            
            // 决定是否抢球 - 如果球靠近且没有队友明显更接近球
            if ((ball_pos - our_players->getPosition(robot_id)).length() < 70) {
                return our_players->createMoveTask(robot_id, ball_pos);
            }
        }
        
        // 创建移动任务，面向球的方向
        return our_players->createMoveTask(robot_id, adjusted_pos, (ball_pos - adjusted_pos).angle());
    }
};

/**
 * @brief 回撤防守战术
 */
class RetreatDefenseTactic : public Tactic {
public:
    RetreatDefenseTactic(const WorldModel* model) : Tactic(model) {}
    
    std::string getName() const override {
        return "RetreatDefense";
    }
    
    TacticType getType() const override {
        return TacticType::DEFENSE;
    }
    
    TacticEvaluation evaluate() const override {
        double score = 0.0;
        std::string desc = "Retreat defense evaluation: ";
        
        // 评估条件1: 如果我方少人或有球员被罚下
        int our_count = 0;
        int opp_count = 0;
        
        for (int i = 0; i < MAX_TEAM_ROBOTS; i++) {
            if (world_model->get_our_exist_id()[i]) our_count++;
            if (world_model->get_opp_exist_id()[i]) opp_count++;
        }
        
        if (our_count < opp_count) {
            score += 3.0;
            desc += "We have fewer players (+3.0); ";
        }
        
        // 评估条件2: 对方进攻球员多，有反击风险
        point2f ball_pos = ball_tools->getPosition();
        if (ball_pos.x > 0) {
            // 球在对方半场
            int opp_attacking_players = 0;
            std::vector<int> opp_ids = opp_players->getPlayerIds();
            
            for (int id : opp_ids) {
                point2f pos = opp_players->getPosition(id);
                if (pos.x < 0) {  // 对方球员在我方半场
                    opp_attacking_players++;
                }
            }
            
            if (opp_attacking_players >= 2) {
                score += 2.0;
                desc += "Multiple opponents positioned for counter-attack (+2.0); ";
            }
        } else {
            // 评估条件3: 如果球在我方半场，而且接近球门
            if (ball_pos.x < -FIELD_LENGTH_H/2) {
                score += 3.0;
                desc += "Ball deep in our half (+3.0); ";
            }
        }
        
        // 总结评分
        desc += "Total score: " + std::to_string(score);
        return TacticEvaluation(score / 10.0, desc);
    }
    
    PlayerTask execute(int robot_id) override {
        point2f ball_pos = ball_tools->getPosition();
        point2f our_goal(-FIELD_LENGTH_H, 0);
        
        // 获取所有球员ID，确定防守顺序
        std::vector<int> player_ids = our_players->getPlayerIds();
        int num_players = player_ids.size();
        
        // 找到当前球员在队伍中的索引
        int player_index = 0;
        for (int i = 0; i < num_players; i++) {
            if (player_ids[i] == robot_id) {
                player_index = i;
                break;
            }
        }
        
        // 根据球员位置计算防守位置
        point2f defense_pos;
        
        // 基础防守位置
        if (player_index % 3 == 0) {
            // 中路防守
            defense_pos = point2f(-FIELD_LENGTH_H + 120, 0);
        } else if (player_index % 3 == 1) {
            // 左路防守
            defense_pos = point2f(-FIELD_LENGTH_H + 150, -100);
        } else {
            // 右路防守
            defense_pos = point2f(-FIELD_LENGTH_H + 150, 100);
        }
        
        // 根据球的位置调整防守位置
        double shift_factor = 0.3;  // 根据球的位置偏移的因子
        
        // 向球的方向偏移，但保持基本阵型
        point2f ball_shift((ball_pos.x - defense_pos.x) * shift_factor, 
                          (ball_pos.y - defense_pos.y) * shift_factor);
        
        // 限制偏移量
        if (ball_shift.length() > 100) {
            ball_shift = ball_shift / ball_shift.length() * 100;
        }
        
        defense_pos = defense_pos + ball_shift;
        
        // 确保防守位置在场地范围内
        defense_pos.x = std::min(std::max(defense_pos.x, -FIELD_LENGTH_H + 30), 0.0);
        defense_pos.y = std::min(std::max(defense_pos.y, -FIELD_WIDTH_H + 30), FIELD_WIDTH_H - 30);
        
        // 如果是最接近球的球员且球在我方半场，考虑去抢球
        if (robot_id == our_players->getClosestPlayerToBall() && 
            ball_pos.x < 0 && 
            (ball_pos - our_players->getPosition(robot_id)).length() < 120) {
            return our_players->createMoveTask(robot_id, ball_pos);
        }
        
        // 面向球的方向
        double orientation = (ball_pos - defense_pos).angle();
        
        return our_players->createMoveTask(robot_id, defense_pos, orientation);
    }
};

#endif // DEFENSE_TACTICS_H 