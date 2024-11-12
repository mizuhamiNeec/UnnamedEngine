#include "MathLib.h"
#include "../Lib/Structs/Structs.h"

#include <cassert>
#include <cmath>
#include <algorithm>

bool Math::IsCollision(const AABB& aabb, const Vec3& point) {
	return (
		point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z
		);
}

Vec3 Math::Lerp(const Vec3& a, const Vec3& b, const float t) {
	return a * (1 - t) + b * t;
}

Vec3 Math::CatmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

Vec3 Math::CatmullRomPosition(const std::vector<Vec3>& points, float t) {
	assert(points.size() >= 4 && "制御点は4点以上必要です");

	// 区間数は制御点の数-1
	size_t division = points.size() - 1;
	// 1区間の長さ(全体を1.0とした割合)
	float areaWidth = 1.0f / division;

	// 区間内の始点を0.0f,終点を1.0fとしたときの現在位置
	float t_2 = std::fmod(t, areaWidth) * division;
	// 下限(0.0f)と上限(1.0f)の範囲に収める
	t_2 = std::clamp(t_2, 0.0f, 1.0f);

	// 区間番号
	size_t index = static_cast<size_t>(t / areaWidth);
	// 区間番号が上限を超えないように収める
	if (index >= division) {
		index = division - 1;
	}

	// 4点分のインデックス
	size_t index0 = index;
	if (index > 0) {
		index0 = index - 1;
	}
	size_t index1 = index;
	size_t index2 = index + 1;
	size_t index3 = index2;
	if (index3 + 1 < points.size()) {
		index3 = index2 + 1;
	}

	// 4点の座標
	const Vec3& p0 = points[index0];
	const Vec3& p1 = points[index1];
	const Vec3& p2 = points[index2];
	const Vec3& p3 = points[index3];

	// 4点を指定してCatmull-Rom補完
	return CatmullRomInterpolation(p0, p1, p2, p3, t_2);
}

Vec3 Math::CatmullRomInterpolation(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
	const float t2 = t * t;
	const float t3 = t2 * t;

	// Catmull-Rom スプラインの補間式
	return 0.5f * (
		(2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
		);
}

float Math::CalculateSplineLength(const std::vector<Vec3>& controlPointsA, const int numSamples) {
	float length = 0.0f;
	Vec3 prevPos = Math::CatmullRomPosition(controlPointsA, 0.0f);
	for (int i = 1; i <= numSamples; ++i) {
		float t = static_cast<float>(i) / numSamples;
		Vec3 currentPos = Math::CatmullRomPosition(controlPointsA, t);
		length += (currentPos - prevPos).Length();
		prevPos = currentPos;
	}
	return length;
}
