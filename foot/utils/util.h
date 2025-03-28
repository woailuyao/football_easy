/*========================================================================
    Util.h : Numerical utility functions
  ------------------------------------------------------------------------
    Copyright (C) 1999-2002  James R. Bruce
    School of Computer Science, Carnegie Mellon University
  ------------------------------------------------------------------------
    This software is distributed under the GNU General Public License,
    version 2.  If you do not have a copy of this licence, visit
    www.gnu.org, or write: Free Software Foundation, 59 Temple Place,
    Suite 330 Boston, MA 02111-1307 USA.  This program is distributed
    in the hope that it will be useful, but WITHOUT ANY WARRANTY,
    including MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  ========================================================================*/

#ifndef __UTIL_H__
#define __UTIL_H__

#include <cmath>
#include <algorithm>
using namespace std;

#ifndef M_2PI
#define M_2PI 6.28318530717958647693
#endif

#ifndef M_PI
#define M_PI 3.1415926
#endif
// 将x限制在low与high之间
template <class num1,class num2>
inline num1 bound(num1 x,num2 low,num2 high)
{
	if(x < low ) x = low;
	if(x > high) x = high;
	return(x);
}

// bound absolute value x in [-range,range]
template <class num1,class num2>
inline num1 abs_bound(num1 x,num2 range)
{
	if(x < -range) x = -range;
	if(x >  range) x =  range;
	return(x);
}

template <class num>
inline num max3(num a,num b,num c)
{
	if(a > b){
		return((a > c)? a : c);
	}else{
		return((b > c)? b : c);
	}
}

template <class num>
inline num min3(num a,num b,num c)
{
	if(a < b){
		return((a < c)? a : c);
	}else{
		return((b < c)? b : c);
	}
}

template <class num>
inline num max_abs(num a,num b)
{
	return((fabs(a) > fabs(b))? a : b);
}

template <class num>
inline num min_abs(num a,num b)
{
	return((fabs(a) < fabs(b))? a : b);
}

// 排序: 小->大
template <class num>
inline void sort(num &a,num &b,num &c)
{
	if(a > b) swap(a,b);
	if(b > c) swap(b,c);
	if(a > b) swap(a,b);
}

template <class real>
real sq(real x)
{
	return(x * x);
}

template <class num>
num sign_nz(num x)
{
	return((x >= 0)? 1 : -1);
}

template <class num>
num sign(num x)
{
	return((x >= 0)? (x > 0) : -1);
}

// 求余数
// Does a real modulus the *right* way, using
// truncation instead of round to zero.
template <class real>
real fmodt(real x,real m)
{
	return(x - floor(x / m)*m);
}

// Returns angle within [-PI,PI]
// Returns angle within [-PI,PI]
/*这段代码定义了一个函数 angle_mod，它接受一个浮点数 a 作为输入，并返回这个数相对于 2π（即 360 度）的模（或余数）。这通常用于将角度标准化到 [0, 2π) 的范围内，或者在更一般的数学上下文中，将任何实数标准化到特定周期的范围内。

具体来说，函数的实现方式是：

a / M_2PI：计算 a 除以 2π 的值。这里 M_2PI 应该是预先定义的常量，表示 2π 的值。

rint(a / M_2PI)：使用 rint 函数对 a / M_2PI 的结果进行四舍五入到最接近的整数。rint 函数返回最接近的整数，当值恰好在两个整数之间时，它会根据该值的最后一位小数进行四舍五入。

M_2PI * rint(a / M_2PI)：将四舍五入后的整数乘以 2π，得到 a 中最接近的 2π 的倍数的值。

a -= M_2PI * rint(a / M_2PI)：从 a 中减去这个最接近的 2π 的倍数，得到 a 相对于 2π 的余数。

return(a);：返回这个余数。

这样，无论 a 的初始值是多少，angle_mod 函数都会返回一个在 [0, 2π) 范围内的数。例如，如果 a 是 7π（即 1260 度），那么 angle_mod(a) 将返回 π（即 180 度），因为 7π 减去最接近的 2π 的倍数（即 6π 或 1080 度）等于 π。*/
template <class real>
real angle_mod(real a)
{
	a -= M_2PI * rint(a / M_2PI);

	return(a);
}

// Returns angle within [-PI,PI]
template <class real>
real anglemod(real a)
{
	a -= M_2PI * rint(a / M_2PI);

	return(a);
}

template <class real>
real angle_rorate(real a){
	a -= M_2PI * rint(a / M_2PI);
	if (a <= M_PI && a > 0) return (a - M_PI);
	else return (a + M_PI);
}

// Returns difference of two angles (a-b), within [-PI,PI]
template <class real>
real angle_diff(real a,real b)
{
	real d;

	d = a - b;
	d -= M_2PI * rint(d / M_2PI);

	return(d);
}

template <class data>
inline int mcopy(data *dest,data *src,int num)
{
	int i;

	for(i=0; i<num; i++) dest[i] = src[i];

	return(num);
}

template <class data>
inline data mset(data *dest,data val,int num)
{
	int i;

	for(i=0; i<num; i++) dest[i] = val;

	return(val);
}

template <class data>
inline void mzero(data &d)
{
	memset(&d,0,sizeof(d));
}

template <class data>
inline void mzero(data *d,int n)
{
	memset(d,0,sizeof(data)*n);
}

template <class node>
int list_length(node *list)
{
	node *p = list;
	int num = 0;

	while(p){
		num++;
		p = p->next;
	}

	return(num);
}

template <class num>
num angle_all(num x, num y) 
{
	x = (fabs(x) < EPSILON)? 0.0f:x;
	y = (fabs(y) < EPSILON)? 0.0f:y;
	if (x == 0) {
		if (y >= 0) {
			return M_2PI/4;
		} else 	{
			return M_2PI*3/4;
		}
	} else {
		return fabs(sign(y)*sign(x))*
					((1-sign(y))*(1+sign(x))/4*M_2PI+
						(1+sign(y))*(1-sign(x))/8*M_2PI+
						(1-sign(y))*(1-sign(x))/8*M_2PI)+
						(1+sign(y))*(1-sign(y))*(1-sign(x))/4*M_2PI+
						atan(y/x);
	}
}

#endif
