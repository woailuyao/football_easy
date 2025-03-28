#include "maths.h"


//命名空间，场地上的不同点
namespace FieldPoint{
	//我方球门中心点
	point2f Goal_Center_Point(-FIELD_LENGTH / 2, 0.0);      
	//罚球点
	point2f Penalty_Kick_Point(-FIELD_LENGTH / 2 + PENALTY_KICKER_L, 0.0);   
	//球门的左角点
	point2f Goal_Left_Point(-FIELD_LENGTH_H, -GOAL_WIDTH_H);
	//球门的右角点
	point2f Goal_Right_Point(-FIELD_LENGTH_H, GOAL_WIDTH_H);
	//球门中心线左侧的两个点，可能是用于定义罚球区的边界。
	point2f Goal_Center_Left_One_Point(-FIELD_LENGTH / 2, -PENALTY_BISECTOR);
	point2f Goal_Center_Left_Two_Point(-FIELD_LENGTH / 2, -PENALTY_BISECTOR * 2);
	//球门中心线右侧的两个点，同样可能是用于定义罚球区的边界。
	point2f Goal_Center_Right_One_Point(-FIELD_LENGTH / 2, PENALTY_BISECTOR);
	point2f Goal_Center_Right_Two_Point(-FIELD_LENGTH / 2, PENALTY_BISECTOR * 2);
	//罚球区（禁区）的左右两个边界点，位于球门线前方
	point2f Goal_Penalty_Area_L_Right(-FIELD_LENGTH, PENALTY_AREA_L / 2);
	point2f Goal_Penalty_Area_L_Left(-FIELD_LENGTH, -PENALTY_AREA_L / 2);

	point2f Penalty_Area_L_A(PENALTY_AREA_R - FIELD_LENGTH_H, PENALTY_AREA_L / 2);
	point2f Penalty_Area_L_B(PENALTY_AREA_R - FIELD_LENGTH_H, -PENALTY_AREA_L / 2);
	//罚球弧的中心点，通常用于定义罚球时球员必须站在其后的弧线。
	point2f Penalty_Arc_Center_Right(-FIELD_LENGTH_H, PENALTY_AREA_L/2);
	point2f Penalty_Arc_Center_Left(-FIELD_LENGTH_H,-PENALTY_AREA_L/2);
	//可能是罚球区内某个矩形的边界点，这个矩形可能与罚球时的站位规则有关。
	point2f Penalty_Rectangle_Left(-FIELD_LENGTH_H + PENALTY_AREA_R, -PENALTY_AREA_L / 2);
	point2f Penalty_Rectangle_Right(-FIELD_LENGTH_H + PENALTY_AREA_R, PENALTY_AREA_L / 2);
}


// 定义了一个名为 Maths 的命名空间，用于封装与数学计算相关的函数。  
namespace Maths
{

	// 归一化角度函数，将角度值限制在 [-PI, PI) 范围内。  
	float normalize (float theta) {
		// 如果角度已经在 [-PI, PI) 范围内，则直接返回。  
		if ( theta >= -PI && theta < PI )
			return theta;

		// 计算角度需要减去的完整圆周的倍数。  
		int multiplier = (int)(theta / (2 * PI));

		// 从原始角度中减去完整圆周的倍数。  
		theta = theta - multiplier * 2 * PI;

		// 如果调整后的角度大于或等于 PI，则减去 2*PI，将其调整到 [-PI, PI) 范围内。  
		if ( theta >= PI )
			theta -= 2 * PI;

		// 如果调整后的角度小于 -PI，则加上 2*PI，将其调整到 [-PI, PI) 范围内。  
		if ( theta < -PI )
			theta += 2 * PI;

		// 返回归一化后的角度。  
		return theta;
	}

	// 将极坐标转换为二维笛卡尔坐标的函数。  inline是C++语言中的一个关键字，用于定义内联函数。内联函数在调用时，不是通过函数调用的机制来实现，而是通过将函数体直接插入到调用处来执行，这样可以大大减少由函数调用带来的开销，从而提高程序的运行效率。在函数声明或定义中，通过在函数返回类型前加上关键字inline，可以将函数指定为内联函数。然而，要注意的是，关键字inline必须与函数定义放在一起，才能使函数成为内联函数，仅仅将inline放在函数声明前面并不起作用。
	inline point2f vector2polar (float length, float dir) {
		// 根据极坐标的长度和方向计算笛卡尔坐标。  
		return point2f (length * cos (dir), length * sin (dir));
	}

	// 计算从起点到沿指定方向延伸特定半径长度的点的函数。  
	// 注意：这并不是计算线段与圆的交点，而是计算从起点沿特定方向移动特定距离的点。  
	point2f circle_segment_intersection (const point2f& start_point, const double circle_r, const point2f& end_point) {
		// 计算从起点到终点的方向角度。  
		// 这里假设angle函数返回的是从x轴到该点向量的角度。  
		float orientation_ang = (end_point - start_point).angle ( );

		// 使用极坐标到笛卡尔坐标的转换函数（假设已定义）  
		// 在起点上，沿着计算出的方向角度移动圆的半径长度，得到一个新的点。  
		// 注意：这个函数并不是找到线段与圆的交点，而是简单地将半径向量加到起点上。  
		return start_point + vector2polar (circle_r, orientation_ang);
	}
	// 计算阿基米德螺旋上的点的函数。  
	point2f archimedes_spiral (const point2f& spiral_center, float spiral_center_size, const point2f& pos, float spiral_buff) {
		point2f spiral_point;

		// 计算给定点到螺旋中心的距离。  
		float dist = (pos - spiral_center).length ( );

		// 计算给定点到螺旋中心的方向角度。  
		float angle = (pos - spiral_center).angle ( );

		// 定义距离步长（这里看起来是硬编码的，可能需要根据实际情况调整）。  
		float dist_step = 8.0f;  //依据旋转180，5次旋转成功，对应dist_step 为40/5-- fuck 执行和思路有点偏差 先不管

		// 定义角度步长（这里似乎有错误，应该是将角度转换为度，然后减去36度，但代码写成了乘以36）。  
		float ang_step = PI / 180 * 36;

		// 归一化角度，并减去角度步长。  
		float ang_deta = Maths::normalize (angle - ang_step);

		// 计算距离差值，并确保它不小于螺旋中心大小加上缓冲区大小。  
		float dist_deta = dist - dist_step;
		if ( dist_deta < spiral_center_size + spiral_buff )
			dist_deta = spiral_center_size + spiral_buff;

		// 使用计算出的距离差值和角度差值，得到阿基米德螺旋上的点。  
		spiral_point = spiral_center + Maths::vector2polar (dist_deta, ang_deta);

		// 返回螺旋上的点。  
		return spiral_point;
	}

	/*point2f archimedes_spiral(const point2f& spiral_center, float spiral_center_size, const point2f& pos, float spiral_buff, int num_points) {
	float initial_dist = (pos - spiral_center).length(); // 初始距离
	float initial_angle = atan2(pos.y - spiral_center.y, pos.x - spiral_center.x); // 初始角度
	float angle_step = 2 * PI / num_points; // 每次迭代的角度变化量
	float dist_step = (initial_dist - (spiral_center_size + spiral_buff)) / num_points; // 每次迭代的距离减少量
	std::vector<point2f> spiral_points; // 存储螺旋点的向量

	for (int i = 0; i < num_points; ++i) {
	float angle = Maths::normalizeAngle(initial_angle - i * angle_step); // 计算当前角度
	float dist = initial_dist - i * dist_step; // 计算当前距离
	if (dist < spiral_center_size + spiral_buff) {
	dist = spiral_center_size + spiral_buff; // 确保距离不小于螺旋中心大小加上缓冲区大小
	}
	point2f spiral_point = spiral_center + Maths::vector2polar(dist, angle); // 计算螺旋点
	spiral_points.push_back(spiral_point); // 将螺旋点添加到向量中
	}

	// 如果只需要一个点（例如，最近的点），则返回最后一个点
	if (num_points == 1) {
	return spiral_points.back();
	}

	// 如果需要多个点，则返回包含所有点的向量（这里需要根据实际需求处理）
	// ...（可能需要返回整个向量或进行其他处理）

	// 作为示例，这里只返回最后一个点
	return spiral_points.back();
	}*/

	// 定义一个函数，用于返回给定浮点数的符号（正数返回1，负数返回-1，）  
	int sign (float d){
		return (d >= 0) ? 1 : -1;
	}

	// 计算从点b到点a和点c之间的向量之间的角度差
	float vector_angle (const point2f& a, const point2f& b, const point2f& c){
		return  (angle_mod ((a - b).angle ( ) - (c - b).angle ( )));
	}

	// 使用最小二乘法计算一组二维点集的最佳拟合直线的角度  
	float least_squares (const vector<point2f>& points){
		float x_mean = 0; // x坐标的均值  
		float y_mean = 0; // y坐标的均值  
		float Dxx = 0, Dxy = 0, Dyy = 0; // 用于计算协方差矩阵的元素  
		float a = 0, b = 0; // 拟合直线的参数  

		// 计算x和y的均值  
		for ( int i = 0; i < points.size ( ); i++ )
		{
			x_mean += points[i].x;
			y_mean += points[i].y;
		}
		x_mean /= points.size ( );
		y_mean /= points.size ( ); // 到此，x和y的均值计算完毕  

		// 计算协方差矩阵的元素  
		for ( int i = 0; i < points.size ( ); i++ )
		{
			Dxx += (points[i].x - x_mean) * (points[i].x - x_mean);
			Dxy += (points[i].x - x_mean) * (points[i].y - y_mean);
			Dyy += (points[i].y - y_mean) * (points[i].y - y_mean);
		}

		// 计算直线参数的lambda和den  
		double lambda = ((Dxx + Dyy) - sqrt ((Dxx - Dyy) * (Dxx - Dyy) + 4 * Dxy * Dxy)) / 2.0;
		double den = sqrt (Dxy * Dxy + (lambda - Dxx) * (lambda - Dxx));

		// 如果分母接近于0，则返回0.0（表示无法计算角度）  
		if ( fabs (den) < 1e-5 )
		{
			return 0.0;
		}
		else
		{
			// 计算直线参数a和b  
			a = Dxy / den;
			b = (lambda - Dxx) / den;
			// 返回拟合直线的角度（使用atan函数计算）  
			return atan (-a / b);
		}
	}

	// 根据给定的点和斜率，计算通过另一点且与该点所在直线垂直的直线上的一个点  
	point2f line_perp_across (const point2f& p1, float slope, const point2f& p2){
		float a, b, b1;

		// 根据斜率判断直线的方向  
		if ( isnan (slope) ){ // 如果斜率不是数（NaN）  
			a = 1;
			b = 0;
		}
		else if ( fabs (slope) < 0.00001 ){ // 如果斜率接近0  
			a = 0;
			b = 1;
		}
		else
		{
			a = slope;
			b = p1.y - a*p1.x; // y = ax + b  根据p1点和斜率算所在直线
		}

		// 计算垂直线的b1（即y = -1/a * x + b1）  
		b1 = -p2.x - a*p2.y;

		// 计算垂直线与原直线的交点  
		point2f across_point;
		across_point.x = (-b1 - a*b) / (a*a + 1);
		across_point.y = a * across_point.x + b;

		// 返回交点  
		return across_point;
	}

	// 判断点p1是否在由左下角点left_down和右上角点right_up定义的矩形范围内  
	bool in_range (const point2f& p1, const point2f& left_down, const point2f& right_up){
		// 检查p1的x坐标是否在left_down和right_up的x坐标之间  
		// 检查p1的y坐标是否在left_down和right_up的y坐标之间  
		return (p1.x < right_up.x && p1.x > left_down.x) && (p1.y < right_up.y && p1.y > left_down.y);
	}

	// 根据两组点p1,p2和q1,q2计算一个交点，这个交点位于p1和p2的连线上，且根据q1和q2到p1,p2连线的距离进行加权  
	point2f across_point (const point2f &p1, const point2f &p2, const point2f &q1, const point2f &q2){
		// 计算q1到线段p1-p2的垂足（投影点）  
		point2f project_point_a = point_on_segment (p1, p2, q1, true);
		// 计算q2到线段p1-p2的垂足（投影点）  
		point2f project_point_b = point_on_segment (p1, p2, q2, true);

		// 计算q1到其投影点的距离  
		float a = (q1 - project_point_a).length ( );
		// 计算q2到其投影点的距离  
		float b = (q2 - project_point_b).length ( );

		// 根据a和b的距离加权计算最终的交点位置  
		// 该方法通常用于计算两线段之间的近似交点或根据权重进行插值  
		return (project_point_b*(a / (a + b)) + project_point_a*(b / (a + b)));
	}

	// 判断球的位置是否在罚球区内  
	bool is_inside_penatly (const point2f& ball){
		// 如果球的x坐标的绝对值大于场的一半长度减去罚球区半径，并且球的x坐标小于0  
		if ( fabs (ball.x) > FIELD_LENGTH_H - PENALTY_AREA_R  && ball.x < 0 ){
			// 如果球的y坐标的绝对值小于罚球区宽度的一半，那么球在罚球区内  
			if ( fabs (ball.y) < PENALTY_AREA_L / 2 )
				return true;
			// 如果球的y坐标小于0，并且球到左侧罚球区边界的距离小于罚球区半径，那么球在罚球区内  
			else if ( ball.y < 0 && (ball - FieldPoint::Goal_Penalty_Area_L_Left).length ( ) < PENALTY_AREA_R )
				return true;
			// 如果球的y坐标大于0，并且球到右侧罚球区边界的距离小于罚球区半径，那么球在罚球区内  
			else if ( ball.y > 0 && (ball - FieldPoint::Goal_Penalty_Area_L_Right).length ( ) < PENALTY_AREA_R )
				return true;
			// 如果以上条件都不满足，那么球不在罚球区内  
			else
				return false;
		}
		// 如果球的x坐标不满足上述条件，那么球肯定不在罚球区内  
		return false;
	}
}