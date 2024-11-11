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

#include "Vector/Vec3.h"

// スプライン曲線制御点(通過点)
static std::vector<Vec3> controlPoints{
	{4.0f * 4.0f, 0.0f * 4.0f, -1.0f * 4.0f},
	{1.0f * 4.0f, 4.0f * 4.0f, 0.0f * 4.0f},
	{1.5f * 4.0f, 1.0f * 4.0f, 1.0f * 4.0f},
	{0.6f * 4.0f, 6.0f * 4.0f, 2.8f * 4.0f},
	{3.7f * 4.0f, 9.0f * 4.0f, 4.6f * 4.0f},
	{-2.2f * 4.0f, 7.0f * 4.0f, 4.4f * 4.0f},
	{-3.1f * 4.0f, 3.0f * 4.0f, 1.2f * 4.0f},
	{1.0f * 4.0f, -5.0f * 4.0f, 7.0f * 4.0f},
	{-0.9f * 4.0f, -1.0f * 4.0f, 9.8f * 4.0f},
	{-5.8f * 4.0f, 4.0f * 4.0f, 7.6f * 4.0f},
	{3.3f * 4.0f, 4.0f * 4.0f, 2.4f * 4.0f},
};

struct AABB;

namespace Math {
	// π!
	constexpr float pi = std::numbers::pi_v<float>;

	// 変換
	constexpr float deg2Rad = pi / 180.0f;
	constexpr float rad2Deg = 180.0f / pi;

	bool IsCollision(const AABB& aabb, const Vec3& point);

	Vec3 Lerp(const Vec3& a, const Vec3& b, float t);
	Vec3 CatmullRomPosition(const std::vector<Vec3>& points, float t);
	Vec3 CatmullRomInterpolation(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);
}