#include <iostream>
#include <string>
#include <cmath>
#include <windows.h>
#include <vector>
#include <chrono>
#include <map>

// 工具头文件
#include "utils/maths.h"
#include "utils/constants.h"
#include "utils/WorldModel.h"
#include "utils/game_state.h"
#include "utils/PlayerTask.h"
#include "utils/vector.h"

// 自定义工具
#include "my_utils/timer.h"
#include "my_utils/ball_tools.h"
#include "my_utils/players.h"
#include "my_utils/goalie.h"
#include "my_utils/opp_players.h"
#include "my_utils/opp_goalie.h"
#include "my_utils/logger.h"
#include "my_utils/communication.h"
#include "my_utils/tactics.h"
#include "my_utils/attack_tactics.h"
#include "my_utils/defense_tactics.h"
#include "my_utils/special_tactics.h"
#include "my_utils/transition_tactics.h"

using namespace std;

// 定义比赛状态常量（如果在special_tactics.h中已有定义，则这些定义将被忽略）
#ifndef PM_NORMAL
#define PM_NORMAL 99
#endif

// 全局变量
static BallTools* ball_tools = nullptr;
static Players* our_players = nullptr;
static OppPlayers* opp_players = nullptr;
static OppGoalie* opp_goalie = nullptr;
static TacticFactory* tactic_factory = nullptr;
static int cycle_counter = 0;
static bool initialized = false;

// 初始化函数
void initialize(const WorldModel* model, int robot_id) {
    // 初始化工具类
    ball_tools = new BallTools(model);
    our_players = new Players(model);
    opp_players = new OppPlayers(model);
    opp_goalie = new OppGoalie(model);
    
    // 初始化日志
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setDebugOutput(true);
    
    // 初始化通信
    Communication::getInstance().initialize(robot_id);
    
    // 初始化战术
    tactic_factory = &TacticFactory::getInstance();
    
    // 注册进攻战术
    tactic_factory->registerTactic(std::make_shared<DirectAttackTactic>(model));
    tactic_factory->registerTactic(std::make_shared<PassAndShootTactic>(model));
    tactic_factory->registerTactic(std::make_shared<WingAttackTactic>(model));
    
    // 注册防守战术
    tactic_factory->registerTactic(std::make_shared<ManMarkingTactic>(model));
    tactic_factory->registerTactic(std::make_shared<ZoneDefenseTactic>(model));
    tactic_factory->registerTactic(std::make_shared<RetreatDefenseTactic>(model));
    
    // 注册特殊战术
    tactic_factory->registerTactic(std::make_shared<KickoffTactic>(model));
    tactic_factory->registerTactic(std::make_shared<FreeKickTactic>(model));
    tactic_factory->registerTactic(std::make_shared<CornerKickTactic>(model));
    
    // 注册转换战术
    tactic_factory->registerTactic(std::make_shared<CounterAttackTactic>(model));
    tactic_factory->registerTactic(std::make_shared<QuickDefenseTactic>(model));
    
    // 标记初始化完成
    initialized = true;
    
    debug_output("Robot 2 initialized, ID: " + std::to_string(robot_id));
}

// 清理函数
void cleanup() {
    if (ball_tools) delete ball_tools;
    if (our_players) delete our_players;
    if (opp_players) delete opp_players;
    if (opp_goalie) delete opp_goalie;
    
    // 清理通信
    Communication::getInstance().cleanup();
    
    // 重置指针
    ball_tools = nullptr;
    our_players = nullptr;
    opp_players = nullptr;
    opp_goalie = nullptr;
    
    debug_output("Robot 2 resources cleaned up");
}

// 获取游戏状态的辅助函数
int getPlayMode(const WorldModel* model) {
    const GameState* game_state = model->game_states();
    if (!game_state) return PM_NORMAL; // 默认为正常比赛状态
    
    // 检查各种状态并返回对应的PlayMode
    if (game_state->gameOff()) return PM_STOP;
    
    if (game_state->ourKickoff()) return PM_OurKickOff;
    if (game_state->theirKickoff()) return PM_OppKickOff;
    
    if (game_state->ourDirectKick() || game_state->ourIndirectKick()) return PM_OurFreeKick;
    if (game_state->theirDirectKick() || game_state->theirIndirectKick()) return PM_OppFreeKick;
    
    if (game_state->ourPenaltyKick()) return PM_OurPenaltyKick;
    if (game_state->theirPenaltyKick()) return PM_OppPenaltyKick;
    
    // 更多状态的映射可以在这里添加
    
    return PM_NORMAL; // 默认为正常比赛状态
}

// 导出函数（主函数）供调用
extern "C" __declspec(dllexport) PlayerTask player_plan(const WorldModel* model, int robot_id) {
    PlayerTask task;
    
    // 获取当前周期并更新周期计数
    cycle_counter++;
    
    // 记录周期开始
    debug_output("===== CYCLE " + std::to_string(cycle_counter) + " START =====");
    
    // 初始化工具类（如果尚未初始化）
    if (!initialized) {
        initialize(model, robot_id);
    }
    
    try {
        // 获取当前比赛状态
        int play_mode = getPlayMode(model);
        
        // 特殊情况处理：如果处于停止状态，停在原地
        if (play_mode == PM_STOP) {
            task.target_pos = our_players->getPosition(robot_id);
            task.orientate = our_players->getOrientation(robot_id);
            debug_output("Game stopped, robot " + std::to_string(robot_id) + " holding position");
            return task;
        }
        
        // 正常比赛逻辑
        point2f ball_pos = ball_tools->getPosition();
        point2f player_pos = our_players->getPosition(robot_id);
        
        // 交换球员状态信息
        bool has_ball = our_players->canHoldBall(robot_id);
        Communication::getInstance().broadcastBallPossession(has_ball, ball_pos);
        
        // 检查是否收到了传球意图
        Message pass_msg = Communication::getInstance().receiveMessage(MessageType::PASS_INTENTION);
        if (pass_msg.receiver_id == robot_id) {
            // 收到传球意图，移动到传球接应位置
            debug_output("Received pass intention, moving to reception position, robot " + std::to_string(robot_id));
            task = our_players->createMoveTask(robot_id, pass_msg.position);
            debug_output("===== CYCLE " + std::to_string(cycle_counter) + " END =====");
            return task;
        }
        
        // 检查特殊情况
        TacticType special_tactic_type = TacticType::SPECIAL_SITUATION;
        std::shared_ptr<Tactic> special_tactic = tactic_factory->selectBestTactic(special_tactic_type);
        
        if (special_tactic && special_tactic->evaluate().score > 0.5) {
            // 存在高评分的特殊情况战术，执行它
            debug_output("Executing special tactic: " + special_tactic->getName() + ", robot " + std::to_string(robot_id));
            task = special_tactic->execute(robot_id);
            debug_output("===== CYCLE " + std::to_string(cycle_counter) + " END =====");
            return task;
        }
        
        // 检查转换战术
        TacticType transition_tactic_type = TacticType::TRANSITION;
        std::shared_ptr<Tactic> transition_tactic = tactic_factory->selectBestTactic(transition_tactic_type);
        
        if (transition_tactic && transition_tactic->evaluate().score > 0.7) {
            // 存在高评分的转换战术，执行它
            debug_output("Executing transition tactic: " + transition_tactic->getName() + ", robot " + std::to_string(robot_id));
            task = transition_tactic->execute(robot_id);
            debug_output("===== CYCLE " + std::to_string(cycle_counter) + " END =====");
            return task;
        }
        
        // 决定是进攻还是防守
        TacticType tactic_type;
        
        // 简单的进攻/防守判断逻辑
        if (ball_tools->isInOurHalf()) {
            // 球在我方半场，更倾向于防守
            tactic_type = TacticType::DEFENSE;
            debug_output("Ball in our half, switching to defense, robot " + std::to_string(robot_id));
        } else {
            // 球在对方半场，更倾向于进攻
            tactic_type = TacticType::ATTACK;
            debug_output("Ball in opponent half, switching to attack, robot " + std::to_string(robot_id));
        }
        
        // 选择最佳战术
        std::shared_ptr<Tactic> best_tactic = tactic_factory->selectBestTactic(tactic_type);
        
        if (best_tactic) {
            // 执行选择的战术
            debug_output("Executing tactic: " + best_tactic->getName() + ", robot " + std::to_string(robot_id));
            task = best_tactic->execute(robot_id);
        } else {
            // 如果没有找到合适的战术，执行默认行为
            debug_output("No suitable tactic found, using default behavior, robot " + std::to_string(robot_id));
            
            // 如果是最接近球的球员，去抢球
            if (robot_id == our_players->getClosestPlayerToBall()) {
                if (our_players->canHoldBall(robot_id)) {
                    // 已经持球，尝试射门或传球
                    point2f goal_pos(FIELD_LENGTH_H, 0);
                    double dist_to_goal = (player_pos - goal_pos).length();
                    
                    if (dist_to_goal < 200) {
                        // 接近球门，尝试射门
                        task = our_players->createShootTask(robot_id);
                        debug_output("Robot " + std::to_string(robot_id) + " shooting at goal");
                    } else {
                        // 寻找可能的传球目标
                        std::vector<int> teammates = our_players->getPlayerIds();
                        int pass_target = -1;
                        
                        for (int id : teammates) {
                            if (id != robot_id && our_players->isInOpponentHalf(id)) {
                                pass_target = id;
                                break;
                            }
                        }
                        
                        if (pass_target >= 0) {
                            // 有传球目标，执行传球
                            task = our_players->createPassTask(robot_id, pass_target);
                            debug_output("Robot " + std::to_string(robot_id) + " passing to robot " + std::to_string(pass_target));
                        } else {
                            // 没有传球目标，带球前进
                            point2f target = player_pos;
                            target.x += 100;  // 向前方移动100厘米
                            
                            // 确保目标在场地范围内
                            if (target.x > FIELD_LENGTH_H - 30.0) {
                                target.x = FIELD_LENGTH_H - 30.0;
                            }
                            
                            task = our_players->createDribbleTask(robot_id, target);
                            debug_output("Robot " + std::to_string(robot_id) + " dribbling forward");
                        }
                    }
                } else {
                    // 未持球，移动到球的位置
                    task = our_players->createMoveTask(robot_id, ball_pos);
                    debug_output("Robot " + std::to_string(robot_id) + " moving to ball");
                }
            } else {
                // 不是最接近球的球员，寻找战术位置
                point2f strategic_pos;
                
                // 如果球在对方半场，找进攻位置
                if (!ball_tools->isInOurHalf()) {
                    strategic_pos = point2f(ball_pos.x + 50, ball_pos.y * 0.7);
                    
                    // 确保位置在场地范围内
                    if (strategic_pos.x > FIELD_LENGTH_H - 30.0) {
                        strategic_pos.x = FIELD_LENGTH_H - 30.0;
                    }
                    
                    debug_output("Robot " + std::to_string(robot_id) + " taking offensive position");
                } else {
                    // 球在我方半场，找防守位置
                    strategic_pos = point2f(-FIELD_LENGTH_H/2 + 100, ball_pos.y * 0.5);
                    debug_output("Robot " + std::to_string(robot_id) + " taking defensive position");
                }
                
                // 确保位置在场地范围内
                if (strategic_pos.y < -FIELD_WIDTH_H + 30.0) {
                    strategic_pos.y = -FIELD_WIDTH_H + 30.0;
                } else if (strategic_pos.y > FIELD_WIDTH_H - 30.0) {
                    strategic_pos.y = FIELD_WIDTH_H - 30.0;
                }
                
                task = our_players->createMoveTask(robot_id, strategic_pos);
            }
        }
    } catch (const std::exception& e) {
        // 处理异常
        debug_output("Exception in player_plan: " + std::string(e.what()) + ", robot " + std::to_string(robot_id));
        
        // 异常情况下的默认行为：保持在当前位置
        task.target_pos = our_players->getPosition(robot_id);
        task.orientate = our_players->getOrientation(robot_id);
    }
    
    // 记录周期结束
    debug_output("===== CYCLE " + std::to_string(cycle_counter) + " END =====");
    
    return task;
}




