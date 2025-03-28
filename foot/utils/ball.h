#ifndef BALL_H
#define BALL_H
#include "FilteredObject.h"
#include "historylogger.h"
class Ball
{
public:
	Ball();
	~Ball();
//设置函数
	void set_ball_vision (const point2f pos, bool is_lost); //根据观测到的球的位置pos和球是否被丢失is_lost来更新球的状态。
	void set_cycle(int cycle){ cur_cycle = cycle; }   //设置当前周期或迭代次数。
//更新和预测函数
	void filter_vel(const point2f observe_pos, bool is_lost);  //根据观测到的位置observe_pos和球是否被丢失is_lost来过滤或计算球的速度。
	point2f predicte_pos(const BallVision last_cycle);  //根据上一周期的球状态last_cycle来预测当前周期的球的位置。
	void predicte_ball(bool is_lost);   //根据球是否被丢失来预测球的状态
//获取函数
	const BallVision& get_ball_vision() { return log.getLogger(cur_cycle); }  //获取当前周期的球视觉信息
	const point2f& get_pos(int cycle){ return log.getLogger(cur_cycle -cycle).pos; }  //获取指定周期前的球的位置。
	const point2f& get_pos(){ return log.getLogger(cur_cycle).pos; }  //获取当前周期的球的位置
	const point2f& get_vel ( ){ return vel; }   //获取球的速度    //例子：const point2f& ball_vel = ball.get_vel(); // 获取球的速度的引用  
// 使用ball_vel，但不能修改它
public:
	int lost_frame;
private:
	FilteredObject ball_filter;  //一个FilteredObject类型的对象，可能用于对球的位置或速度进行滤波处理，以平滑噪声或提高精度。
	BallLogger log;  //一个BallLogger类型的对象，用于记录球的状态历史。这可能包括位置、速度和其他相关信息。
	point2f vel;
	int proc_frame;  //表示已处理的帧数或周期数。
	int cur_cycle;
};


#endif