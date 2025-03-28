#include <iostream>
#include <cmath>
#include <vector>
#include <windows.h>
#include <chrono>
#include <algorithm>

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
#include "my_utils/transition_tactics.h"

// 全局变量
static BallTools* ball_tools = nullptr;
static Players* our_players = nullptr;
static OppPlayers* opp_players = nullptr;
static OppGoalie* opp_goalie = nullptr;
static TacticFactory* tactic_factory = nullptr;
static int cycle_counter = 0;
static bool initialized = false;

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
    
    return PM_NORMAL; // 默认为正常比赛状态
}

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
    
    // 注册转换战术
    tactic_factory->registerTactic(std::make_shared<CounterAttackTactic>(model));
    
    // 标记初始化完成
    initialized = true;
    
    debug_output("Robot 1 (Forward) initialized, ID: " + std::to_string(robot_id));
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
    
    debug_output("Robot 1 resources cleaned up");
}

// 寻找射门机会
bool lookForShotOpportunity(int robot_id, PlayerTask& task) {
    point2f player_pos = our_players->getPosition(robot_id);
    point2f goal_pos(FIELD_LENGTH_H, 0);
    
    // 距离球门的距离
    double dist_to_goal = (player_pos - goal_pos).length();
    
    // 如果距离球门足够近且持有球
    if (dist_to_goal < 250 && our_players->canHoldBall(robot_id)) {
        // 评估射门难度
        double shot_difficulty = opp_goalie->evaluateShootingDifficulty(player_pos);
        
        // 如果射门难度低，直接射门
        if (shot_difficulty < 6.0) {
            task = our_players->createShootTask(robot_id);
            return true;
        }
        
        // 寻找最佳射门角度
        double best_angle = opp_goalie->findBestShootingAngle(player_pos);
        
        // 如果有合适的射门角度，调整位置后射门
        if (best_angle != 0.0) {
            point2f adjust_pos = player_pos + point2f(10, best_angle > 0 ? 20 : -20);
            task = our_players->createDribbleTask(robot_id, adjust_pos);
            return true;
        }
    }
    
    return false;
}

// 寻找传球机会
bool lookForPassOpportunity(int robot_id, PlayerTask& task) {
    point2f player_pos = our_players->getPosition(robot_id);
    
    // 如果持有球
    if (our_players->canHoldBall(robot_id)) {
        std::vector<int> teammates = our_players->getPlayerIds();
        int best_target = -1;
        double best_score = -1;
        
        // 评估所有队友作为传球目标
        for (int id : teammates) {
            if (id == robot_id) continue;
            
            point2f teammate_pos = our_players->getPosition(id);
            double pass_score = 0.0;
            
            // 计算传球得分
            // 1. 距离适中的传球
            double dist = (teammate_pos - player_pos).length();
            if (dist > 50 && dist < 300) {
                pass_score += 3.0;
            } else {
                pass_score += 1.0;
            }
            
            // 2. 队友在对方半场
            if (our_players->isInOpponentHalf(id)) {
                pass_score += 2.0;
            }
            
            // 3. 队友离对方球门更近
            point2f goal_pos(FIELD_LENGTH_H, 0);
            double teammate_to_goal = (teammate_pos - goal_pos).length();
            double self_to_goal = (player_pos - goal_pos).length();
            
            if (teammate_to_goal < self_to_goal) {
                pass_score += 2.0;
            }
            
            // 4. 检查传球路线是否畅通
            bool clear_path = true;
            for (int opp_id : opp_players->getPlayerIds()) {
                point2f opp_pos = opp_players->getPosition(opp_id);
                double dist_to_line = distanceToLine(opp_pos, player_pos, teammate_pos);
                if (dist_to_line < 25) {
                    clear_path = false;
                    break;
                }
            }
            
            if (clear_path) {
                pass_score += 3.0;
            } else {
                pass_score -= 2.0;
            }
            
            // 更新最佳传球目标
            if (pass_score > best_score) {
                best_score = pass_score;
                best_target = id;
            }
        }
        
        // 如果找到合适的传球目标
        if (best_target >= 0 && best_score > 5.0) {
            task = our_players->createPassTask(robot_id, best_target);
            return true;
        }
    }
    
    return false;
}

// 计算点到线段的距离
double distanceToLine(const point2f& p, const point2f& a, const point2f& b) {
    point2f AB = b - a;
    point2f AP = p - a;
    
    double ab2 = AB.x * AB.x + AB.y * AB.y;
    double ap_dot_ab = AP.x * AB.x + AP.y * AB.y;
    
    double t = std::max(0.0, std::min(1.0, ap_dot_ab / ab2));
    
    point2f closest = a + AB * t;
    return (p - closest).length();
}

// 导出函数（主函数）供调用
extern "C" __declspec(dllexport) PlayerTask player_plan(const WorldModel* model, int robot_id) {
	PlayerTask task;
	
    // 获取当前周期并更新周期计数
	cycle_counter++;
	
    // 记录周期开始
    debug_output("===== CYCLE " + std::to_string(cycle_counter) + " START (Forward) =====");
    
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
            return task;
        }
        
        // 前锋主要使用进攻和反击战术
        if (ball_tools->isInOurHalf() && ball_tools->getVelocity().x > 50) {
            // 球在我方半场但正在向前移动，考虑反击
            std::shared_ptr<Tactic> counter_tactic = tactic_factory->getTacticByName("Counter Attack Tactic");
            if (counter_tactic && counter_tactic->evaluate().score > 0.6) {
                debug_output("Executing counter attack tactic, robot " + std::to_string(robot_id));
                task = counter_tactic->execute(robot_id);
                return task;
            }
        }
        
        // 如果球在对方半场，使用进攻战术
        if (ball_tools->isInOpponentHalf()) {
            // 选择最佳进攻战术
            std::shared_ptr<Tactic> best_attack = tactic_factory->selectBestTactic(TacticType::ATTACK);
            if (best_attack && best_attack->evaluate().score > 0.5) {
                debug_output("Executing " + best_attack->getName() + " tactic, robot " + std::to_string(robot_id));
                task = best_attack->execute(robot_id);
                return task;
            }
        }
        
        // 如果没有合适的战术，使用基本进攻行为
        
        // 1. 如果是最接近球的球员，去抢球
        if (robot_id == our_players->getClosestPlayerToBall()) {
            if (our_players->canHoldBall(robot_id)) {
                // 已经持球，检查射门机会
                if (lookForShotOpportunity(robot_id, task)) {
                    debug_output("Found shooting opportunity, robot " + std::to_string(robot_id));
                    return task;
                }
                
                // 检查传球机会
                if (lookForPassOpportunity(robot_id, task)) {
                    debug_output("Found passing opportunity, robot " + std::to_string(robot_id));
                    return task;
                }
                
                // 没有好的射门或传球机会，带球向前
                point2f target = player_pos;
                target.x += 100;  // 向前移动100厘米
                
                // 确保目标在场地范围内
                if (target.x > FIELD_LENGTH_H - 30.0) {
                    target.x = FIELD_LENGTH_H - 30.0;
                }
                
                // 避开对手
                for (int opp_id : opp_players->getPlayerIds()) {
                    point2f opp_pos = opp_players->getPosition(opp_id);
                    if ((opp_pos - target).length() < 50) {
                        // 有对手在前进路线上，调整路线
                        if (opp_pos.y > target.y) {
                            target.y -= 30;  // 向下调整
                        } else {
                            target.y += 30;  // 向上调整
                        }
                    }
                }
                
                task = our_players->createDribbleTask(robot_id, target);
                debug_output("No immediate opportunities, dribbling forward, robot " + std::to_string(robot_id));
                return task;
            } else {
                // 未持球，移动到球的位置
                task = our_players->createMoveTask(robot_id, ball_pos);
                debug_output("Moving to ball, robot " + std::to_string(robot_id));
                return task;
            }
        } else {
            // 不是最接近球的球员，找进攻位置
            point2f strategic_pos;
            
            // 如果球在对方半场，找前插位置
            if (ball_tools->isInOpponentHalf()) {
                // 找一个靠近球门但不越位的位置
                strategic_pos = point2f(ball_pos.x + 80, ball_pos.y * 0.5);
                
                // 确保不要太靠近对方球门
                if (strategic_pos.x > FIELD_LENGTH_H - 50.0) {
                    strategic_pos.x = FIELD_LENGTH_H - 50.0;
                }
                
                // 避开其他队友
                for (int id : our_players->getPlayerIds()) {
                    if (id != robot_id) {
                        point2f other_pos = our_players->getPosition(id);
                        if ((other_pos - strategic_pos).length() < 70) {
                            // 有队友在附近，调整位置
                            if (other_pos.y > strategic_pos.y) {
                                strategic_pos.y -= 40;
                            } else {
                                strategic_pos.y += 40;
                            }
                        }
                    }
                }
                
                debug_output("Taking offensive position, robot " + std::to_string(robot_id));
            } else {
                // 球在我方半场，找一个稍微靠前的中场位置
                strategic_pos = point2f(0, ball_pos.y * 0.7);
                debug_output("Taking midfield position, robot " + std::to_string(robot_id));
            }
            
            // 确保位置在场地范围内
            if (strategic_pos.y < -FIELD_WIDTH_H + 30.0) {
                strategic_pos.y = -FIELD_WIDTH_H + 30.0;
            } else if (strategic_pos.y > FIELD_WIDTH_H - 30.0) {
                strategic_pos.y = FIELD_WIDTH_H - 30.0;
            }
            
            task = our_players->createMoveTask(robot_id, strategic_pos);
            return task;
        }
    } catch (const std::exception& e) {
        // 处理异常
        debug_output("Exception in player_plan: " + std::string(e.what()) + ", robot " + std::to_string(robot_id));
        
        // 异常情况下的默认行为：保持在当前位置
        task.target_pos = our_players->getPosition(robot_id);
        task.orientate = our_players->getOrientation(robot_id);
    }
    
    // 记录周期结束
    debug_output("===== CYCLE " + std::to_string(cycle_counter) + " END (Forward) =====");

	return task;
}

// 当DLL被加载或卸载时清理资源
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // DLL被加载时初始化
            break;
            
        case DLL_PROCESS_DETACH:
            // DLL被卸载时清理资源
            cleanup();
            break;
    }
    
    return TRUE;
}




