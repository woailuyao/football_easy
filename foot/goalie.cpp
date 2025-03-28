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
#include "my_utils/goalie.h"
#include "my_utils/ball_tools.h"
#include "my_utils/logger.h"

using namespace std;
// Windows调试输出函数
inline void debug(const std::string& message) {
    OutputDebugStringA((message + "\n").c_str());
}


// 导出函数（主函数）供调用
extern "C" __declspec(dllexport) PlayerTask goalie_plan(const WorldModel* model, int robot_id) {
    // 检查是否是守门员
    if (model->get_our_goalie() != robot_id) {
        // 非守门员，返回空任务
        LOG_WARNING("Robot " + std::to_string(robot_id) + " is not the goalie, but called goalie_plan", robot_id);
        PlayerTask empty_task;
        return empty_task;
    }
    
    // 初始化守门员工具
    static Goalie* goalie = nullptr;
    static BallTools* ball_tools = nullptr;
    
    // 第一次调用时创建工具
    if (!goalie) {
        goalie = new Goalie(model);
        ball_tools = new BallTools(model);
        LOG_INFO("Goalie initialized, ID: " + std::to_string(robot_id), robot_id);
    }
    
    // 开始记录周期
    static int cycle_counter = 0;
    cycle_counter++;
    
    // 记录周期开始
    LOG_CYCLE_START(cycle_counter, robot_id);
    LOG_TIMING_START("goalie_decision");
    
    try {
        // 守门员决策
        PlayerTask task = goalie->decideGoalieTask();
        
        // 记录决策结果
        LOG_POSITION("Goalie target position", task.target_pos, robot_id);
        LOG_ANGLE("Goalie orientation", task.orientate, robot_id);
        if (task.needKick) {
            LOG_INFO("Goalie will kick the ball with power: " + std::to_string(task.kickPower), robot_id);
        }
        
        // 结束计时并记录周期结束
        LOG_TIMING_END("goalie_decision", robot_id);
        LOG_CYCLE_END(cycle_counter, robot_id);
        
        return task;
    } catch (const std::exception& e) {
        // 处理异常
        LOG_ERROR("Exception in goalie_plan: " + std::string(e.what()), robot_id);
        
        // 出现异常时，返回基本防守任务
        PlayerTask task;
        if (goalie) {
            task = goalie->createDefendTask();
        } else {
            // 如果连goalie对象都没有，则守在球门中心
            task.target_pos = point2f(-FIELD_LENGTH_H + 15, 0);
            task.orientate = 0;
        }
        
        // 结束计时并记录周期结束
        LOG_TIMING_END("goalie_decision", robot_id);
        LOG_CYCLE_END(cycle_counter, robot_id);
        
        return task;
    }
}

// 当DLL被加载或卸载时清理资源
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    static Goalie* goalie = nullptr;
    static BallTools* ball_tools = nullptr;
    
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // DLL被加载时初始化
            break;
            
        case DLL_PROCESS_DETACH:
            // DLL被卸载时清理资源
            if (goalie) {
                delete goalie;
                goalie = nullptr;
            }
            if (ball_tools) {
                delete ball_tools;
                ball_tools = nullptr;
            }
            break;
    }
    
    return TRUE;
}




