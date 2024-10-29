#pragma once
#include <numbers>
#include <vector>

#include "Vector/Vec3.h"

// スプライン曲線制御点(通過点)
static std::vector<Vec3> controlPoints_{
	{0,0,0},
	{10,10,10},
	{10,15,20},
	{20,15,30},
	{20,0,40},
	{30,0,50},
	{30,-15,60},
	{40,15,70},
	{40,-15,80},
	{0,0,90},
};

namespace Math {
	// ----------------------------------------------------------------------------
	// 定数
	// ----------------------------------------------------------------------------

	// π
	constexpr float pi = std::numbers::pi_v<float>;

	// Convert degree to radian
	constexpr float deg2Rad = pi / 180.0f;

	// Convert radian to degree
	constexpr float rad2Deg = 180.0f / pi;

	Vec3 CatmullRomPosition(const std::vector<Vec3>& points, float t);
	Vec3 CatmullRomInterpolation(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);
}