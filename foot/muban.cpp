// 足球机器人策略控制模板
// 此文件为策略控制模板，用于编写不同球员的策略
// 主函数player_plan会被系统调用，每秒执行60次

#include <iostream>
#include <string>
#include <cmath>
#include <windows.h>
#include <vector>
#include <chrono>
#include <map>
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

// 比赛状态常量定义（如果在其他文件中已定义，可以注释掉）
#ifndef PM_NORMAL
#define PM_NORMAL 99
#endif
#ifndef PM_STOP
#define PM_STOP 0
#endif
#ifndef PM_OurKickOff
#define PM_OurKickOff 1
#endif
#ifndef PM_OppKickOff
#define PM_OppKickOff 2
#endif
#ifndef PM_OurFreeKick
#define PM_OurFreeKick 9
#endif
#ifndef PM_OppFreeKick
#define PM_OppFreeKick 10
#endif
#ifndef PM_OurPenaltyKick
#define PM_OurPenaltyKick 11
#endif
#ifndef PM_OppPenaltyKick
#define PM_OppPenaltyKick 12
#endif

// 全局变量
static BallTools* ball_tools = nullptr;
static Players* our_players = nullptr;
static OppPlayers* opp_players = nullptr;
static OppGoalie* opp_goalie = nullptr;
static TacticFactory* tactic_factory = nullptr;
static int cycle_counter = 0;
static bool initialized = false;

// Windows调试输出函数
inline void debug(const std::string& message) {
    OutputDebugStringA((message + "\n").c_str());
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
    
    // 在这里注册你需要的战术
    // tactic_factory->registerTactic(std::make_shared<YourTactic>(model));
    
    // 标记初始化完成
    initialized = true;
    
    debug_output("Robot template initialized, ID: " + std::to_string(robot_id));
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
    
    debug_output("Robot template resources cleaned up");
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
        
        // 正常比赛逻辑 - 这里是你需要实现自己机器人行为的地方
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
        
        // ===== 在这里添加你的决策逻辑 =====
        
        // 简单示例: 如果是最接近球的球员，去抢球；否则，找一个合适的位置
        if (robot_id == our_players->getClosestPlayerToBall()) {
            if (our_players->canHoldBall(robot_id)) {
                // 已经持球，可以加入你的控球、传球或射门逻辑
                
                // 简单示例: 带球向前
                point2f target = player_pos;
                target.x += 100;  // 向前移动100厘米
                
                task = our_players->createDribbleTask(robot_id, target);
                debug_output("Dribbling forward, robot " + std::to_string(robot_id));
            } else {
                // 未持球，移动到球的位置
                task = our_players->createMoveTask(robot_id, ball_pos);
                debug_output("Moving to ball, robot " + std::to_string(robot_id));
            }
        } else {
            // 找一个合适的位置
            point2f strategic_pos = point2f(0, 0);  // 示例位置，你需要根据实际情况计算
            task = our_players->createMoveTask(robot_id, strategic_pos);
            debug_output("Moving to strategic position, robot " + std::to_string(robot_id));
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




