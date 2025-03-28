#ifndef WORLDMODEL_H   //#ifndef 检查 BASETASK_H 是否已经被定义。如没有被定义，编译器就会继续处理接下来的代码，直到遇到 #endif
#define WORLDMODEL_H   //如果定义了就删掉代码
#include <iostream>    //ifndef = if no def
#include "singleton.h"
#include "basevision.h"
#include "historylogger.h"
#include "robot.h"
#include "matchstate.h"
#include "ball.h"
#include "game_state.h"
class WorldModel
{
public:
	WorldModel();
	~WorldModel();   //string是一个类
	const std::string& get_referee_msg() const;  //获取裁判发送的消息.表示这个函数返回一个string对象的引用
	void set_referee_msg(const std::string& ref_msg);  //用于设置裁判发送的消息，函数接受一个对 std::string 类型的常量引用作为参数
	void set_cycle(int cycle){ current_cycle = cycle; }   //设置当前的周期。current_cycle类的私有成员变量
	void setMatchState(FieldState state);//设置比赛状态为枚举变量FieldState state
	FieldState getMatchState();    //返回当前的比赛状态，即返回FieldState里的变量
	void set_our_team(Vehicle* team){ our = team; }  //设置我们队伍的车辆指针为 team 。接受一个指向Vehicle类型的指针team作为参数，并将our（类的私有成员变量）设置为这个传入的指针
	const Vehicle* get_our_team()const{ return our; }  //返回our指针的一个常量引用。即返回指向我们队伍车辆的常量指针
	void set_opp_team(Vehicle* team){ opp = team; }    //设置对手队伍的车辆指针为 team
	const Vehicle* get_opp_team()const{ return opp; }  //返回指向对手队伍车辆的常量指针
	void set_our_exist_id(bool * id){ our_robots_id = id; }   //设置一个布尔指针，用于表示我们队伍中每个机器人是否存在的状态。
	const bool* get_our_exist_id()const{ return our_robots_id; }  //返回一个常量布尔指针，表示我们队伍中每个机器人是否存在的状态
	void set_opp_exist_id(bool * id){ opp_robots_id = id; }   //设置对手机器人是否存在的标识数组。opp_robots_id 是一个指向布尔值的指针，表示每个对手机器人是否存在于当前比赛中。
	const bool* get_opp_exist_id()const{ return opp_robots_id; } //获取对手机器人是否存在的标识数组。返回指向布尔值的常量指针
	void set_our_v(int id, point2f v);   //设置我们队伍中指定ID的机器人的速度。point2f 一个表示二维坐标或向量的类型。
	void set_our_rot(int id, float rot);   //设置我们队伍中指定ID的机器人的旋转角度或方向
	void set_our_cmd(int id, const point2f& v, const float& rot);   //设置我们队伍中指定ID的机器人的速度和旋转命令
	const point2f& get_our_cmd_v(int id)const;  //获取我们队伍中指定ID的机器人的速度命令
	const float& get_our_cmd_rot(int id)const;  //获取我们队伍中指定ID的机器人的旋转命令
	const PlayerVision& get_our_player(int id)const;  //获取我们队伍中指定ID的机器人的视野信息，返回一个球员视野的类对象
	const point2f& get_our_player_pos(int id)const;   //获取我们队伍中指定ID的机器人的位置
	const point2f& get_opp_player_pos(int id)const;// 对手位置
	float get_opp_player_dir(int id)const;  //对手方向
	const point2f& get_our_player_v(int id)const;    //获取我们队伍中指定ID的机器人的速度
	const point2f& get_our_player_last_v(int id)const;   //获取我们队伍中指定ID的机器人上一次的速度
	float get_our_player_dir(int id)const;// 获取我们队伍中指定ID的机器人的方向
	const float get_our_player_last_dir(int id)const;  //获取我们队伍中指定ID的机器人上一次的方向
	const PlayerVision& get_opp_player(int id)const;   //对手视野
	int get_our_goalie()const{ return our_goalie; }    //获取我们队伍的守门员ID，，并赋值给我们的球员
	int get_opp_goalie()const{ return opp_goalie; }   //获取对手守门员ID
	void set_our_goalie(int goalie_id){ our_goalie = goalie_id; }  //设置我们队伍的守门员ID
	void set_opp_goalie(int goalie_id){ opp_goalie = goalie_id; }  //设置对手队伍的守门员ID
	void set_ball(Ball* b){ match_ball = b; }  //设置比赛用球的指针。参数是Ball类对象的指针，可能包含了球的位置、速度等信息。
	const BallVision& get_ball()const{ return match_ball->get_ball_vision(); }  //获取球的视野信息。返回的是一个对BallVision类型的常量引用。。match_ball 是一个指向某个对象的指针。这个对象是一个类的实例。有一个名为 get_ball_vision 的方法。-> 是一个指向成员运算符，它用于通过指针访问对象的成员
	const point2f& get_ball_pos(int c)const{ return match_ball->get_pos(c); }  //获取某一帧的位置
	const point2f& get_ball_pos()const{ return match_ball->get_pos(); }   //获取球在这一帧下的位置
	const point2f& get_ball_vel()const{ return match_ball->get_vel(); }  //获取球的速度。返回的是一个对point2f类型的常量引用，表示球的速度的二维向量。
	const int  robots_size()const { return max_robots; }    //获取机器人的最大数量。返回一个整数值
	void set_kick(bool* kick_f) { kick = kick_f; }   //设置踢球状态数组。kick_f是一个指向布尔值的指针数组，用于表示每个机器人是否处于踢球状态。
	void set_sim_kick (bool* kick){ sim_kick = kick; }   //设置模拟踢球状态数组
	const bool* get_sim_kick()const{ return sim_kick; }   //
	bool is_kick(int id)const;        //判断指定ID的机器人是否处于踢球状态
	bool is_sim_kick(int id)const;     //判断指定ID的机器人是否处于模拟踢球状态。
	void set_game_state(GameState* state);  // 设置游戏状态。参数是GameState类对象的指针，包含了当前游戏的各种状态信息。
	const GameState* game_states()const;   // 获取游戏状态。返回指向GameState类型的常量指针，因此不能修改返回的游戏状态对象。
	void set_simulation(bool sim){ is_simulation = sim; } //设置是否为模拟状态。sim是一个布尔值，表示当前是否处于模拟环境中。
	bool get_simulation()const{ return is_simulation; }//获取当前是否为模拟状态。返回一个布尔值，表示是否处于模拟环境中。
private:
	static const int max_robots = 12;
	Vehicle* our;  //一个指向 Vehicle 的指针，代表我们的队伍
	Vehicle* opp; //一个指向 Vehicle 的指针，代表对手的队伍
	bool* kick;   //一个布尔指针，是否踢球
	bool* sim_kick;
	Ball* match_ball;  //一个指向Ball对象的指针
	bool* our_robots_id;  //一个布尔指针，可能用于表示我们队伍中每个机器人是否存在的状态
	bool* opp_robots_id;
	int our_goalie;  //守门员ID
	int opp_goalie;
	int current_cycle;    //表示当前的周期
	std::string referee_msg;  //一个字符串对象，用于存储裁判或系统发送的消息
	FieldState match_state;   //定义一个枚举变量
	GameState* game_state;   //一个指向GameState对象的指针
	bool is_simulation;  //是否处于模拟模式
};
#endif
