#pragma once
#include <numbers>
#include <vector>

#include "Vector/Vec3.h"

struct AABB;
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

namespace Math {
	// π!
	constexpr float pi = std::numbers::pi_v<float>;

	// 変換
	constexpr float deg2Rad = pi / 180.0f;
	constexpr float rad2Deg = 180.0f / pi;

	bool IsCollision(const AABB& aabb, const Vec3& point);
}
