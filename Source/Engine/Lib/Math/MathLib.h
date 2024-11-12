#pragma once
#include <numbers>
#include <vector>

#include "Vector/Vec3.h"

// スプライン曲線制御点(通過点)
static std::vector<Vec3> controlPoints{
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
	Vec3 CatmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);
	Vec3 CatmullRomPosition(const std::vector<Vec3>& points, float t);
	Vec3 CatmullRomInterpolation(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);
	float CalculateSplineLength(const std::vector<Vec3>& controlPointsA, int numSamples);
}
