#ifndef TACTICS_H
#define TACTICS_H

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include "../utils/WorldModel.h"
#include "../utils/PlayerTask.h"
#include "../utils/constants.h"
#include "../utils/vector.h"
#include "ball_tools.h"
#include "players.h"
#include "goalie.h"
#include "opp_players.h"
#include "opp_goalie.h"
#include "logger.h"

// 前向声明解决循环包含问题
class BallTools;
class Players;
class Goalie;
class OppPlayers;
class OppGoalie;

/**
 * @brief 战术类型枚举
 */
enum class TacticType {
    ATTACK,             // 进攻
    DEFENSE,            // 防守
    TRANSITION,         // 过渡
    SPECIAL_SITUATION   // 特殊情况
};

/**
 * @brief 进攻战术枚举
 */
enum class AttackTactic {
    DIRECT_ATTACK,      // 直接进攻
    PASS_AND_SHOOT,     // 传球射门
    WING_ATTACK,        // 边路进攻
    DRIBBLE_ATTACK,     // 盘带进攻
    COUNTER_ATTACK      // 反击
};

/**
 * @brief 防守战术枚举
 */
enum class DefenseTactic {
    MAN_MARKING,        // 人盯人防守
    ZONE_DEFENSE,       // 区域联防
    PRESSURE_DEFENSE,   // 压迫防守
    RETREAT_DEFENSE     // 回撤防守
};

/**
 * @brief 特殊情况战术枚举
 */
enum class SpecialTactic {
    KICKOFF,            // 开球
    FREEKICK,           // 任意球
    PENALTY,            // 点球
    CORNERKICK,         // 角球
    THROWIN             // 界外球
};

/**
 * @brief 战术评估结果结构
 */
struct TacticEvaluation {
    double score;                // 战术评分 (0.0-1.0)
    std::string description;     // 评估描述
    
    TacticEvaluation() : score(0.0), description("") {}
    
    TacticEvaluation(double s, const std::string& desc) : score(s), description(desc) {}
};

/**
 * @brief 战术基类，提供战术评估和执行接口
 */
class Tactic {
public:
    /**
     * @brief 构造函数
     * @param model 世界模型指针
     */
    Tactic(const WorldModel* model)
        : world_model(model), 
          ball_tools(new BallTools(model)),
          our_players(new Players(model)),
          our_goalie(new Goalie(model)),
          opp_players(new OppPlayers(model)),
          opp_goalie(new OppGoalie(model)) {}

    /**
     * @brief 析构函数
     */
    virtual ~Tactic() {
        delete ball_tools;
        delete our_players;
        delete our_goalie;
        delete opp_players;
        delete opp_goalie;
    }
    
    /**
     * @brief 战术名称
     * @return 战术名称字符串
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief 获取战术类型
     * @return 战术类型枚举值
     */
    virtual TacticType getType() const = 0;
    
    /**
     * @brief 评估战术在当前情况下的适用性
     * @return 战术评估结果
     */
    virtual TacticEvaluation evaluate() const = 0;
    
    /**
     * @brief 执行战术，为指定球员生成任务
     * @param robot_id 执行战术的球员ID
     * @return 球员任务
     */
    virtual PlayerTask execute(int robot_id) = 0;

protected:
    const WorldModel* world_model; // 世界模型
    BallTools* ball_tools;         // 球工具
    Players* our_players;          // 我方球员工具
    Goalie* our_goalie;            // 我方守门员工具
    OppPlayers* opp_players;       // 对方球员工具
    OppGoalie* opp_goalie;         // 对方守门员工具
};

/**
 * @brief 战术工厂类，用于创建和管理不同战术
 */
class TacticFactory {
public:
    /**
     * @brief 获取单例实例
     * @return 战术工厂单例
     */
    static TacticFactory& getInstance() {
        static TacticFactory instance;
        return instance;
    }
    
    /**
     * @brief 注册战术
     * @param tactic 战术实例
     */
    void registerTactic(std::shared_ptr<Tactic> tactic) {
        tactics.push_back(tactic);
    }
    
    /**
     * @brief 根据当前情况选择最佳战术
     * @param type 战术类型，默认为任意类型
     * @return 最佳战术实例
     */
    std::shared_ptr<Tactic> selectBestTactic(TacticType type = TacticType::ATTACK) const {
        double best_score = -1.0;
        std::shared_ptr<Tactic> best_tactic = nullptr;
        
        for (const auto& tactic : tactics) {
            // 如果指定了类型，则只评估该类型的战术
            if (tactic->getType() != type) {
                continue;
            }
            
            // 评估战术适用性
            TacticEvaluation eval = tactic->evaluate();
            
            if (eval.score > best_score) {
                best_score = eval.score;
                best_tactic = tactic;
            }
        }
        
        return best_tactic;
    }
    
    /**
     * @brief 根据名称获取战术
     * @param name 战术名称
     * @return 战术实例，如果没找到则返回nullptr
     */
    std::shared_ptr<Tactic> getTacticByName(const std::string& name) const {
        for (const auto& tactic : tactics) {
            if (tactic->getName() == name) {
                return tactic;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief 获取所有战术
     * @return 战术列表
     */
    const std::vector<std::shared_ptr<Tactic>>& getAllTactics() const {
        return tactics;
    }
    
    /**
     * @brief 清除所有战术
     */
    void clearTactics() {
        tactics.clear();
    }

private:
    /**
     * @brief 私有构造函数，保证单例
     */
    TacticFactory() {}
    
    /**
     * @brief 私有析构函数
     */
    ~TacticFactory() {}
    
    // 禁止拷贝和赋值
    TacticFactory(const TacticFactory&) = delete;
    TacticFactory& operator=(const TacticFactory&) = delete;
    
    std::vector<std::shared_ptr<Tactic>> tactics; // 战术列表
};

#endif // TACTICS_H 