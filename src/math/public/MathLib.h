#pragma once

#include <algorithm>
#include <format>
#include <numbers>
#include <xmmintrin.h>

#include <math/public/Vec2.h>
#include <math/public/Vec3.h>
#include <math/public/Vec4.h>

#include <math/public/Mat4.h>
#include <math/public/Quaternion.h>

namespace Math {
	//-------------------------------------------------------------------------
	// 定数
	//-------------------------------------------------------------------------
	constexpr float pi      = std::numbers::pi_v<float>; // π
	constexpr float deg2Rad = pi / 180.0f;               // 度からラジアンへの変換
	constexpr float rad2Deg = 180.0f / pi;               // ラジアンから度への変換
	constexpr float eps     = FLT_EPSILON;

	//-------------------------------------------------------------------------
	// 関数
	//-------------------------------------------------------------------------
	Vec2 Lerp(const Vec2& a, const Vec2& b, float t);
	Vec2 WorldToScreen(
		const Vec3& worldPos, Vec2         screenSize,
		const bool& bClamp, const float&   margin,
		bool&       outIsOffscreen, float& outAngle
	);

	Vec3 ProjectOnPlane(const Vec3& vector, const Vec3& normal);

	Vec3 GetMoveDirection(const Vec3& forward, const Vec3& groundNormal);

	Vec3 Lerp(const Vec3& a, const Vec3& b, float t);
	Vec3 Min(Vec3 a, Vec3 b);
	Vec3 Max(Vec3 a, Vec3 b);

	Vec4 Lerp(const Vec4& a, const Vec4& b, float t);

	float DeltaAngle(float current, float target);

	float CubicBezier(float t, Vec2 p1, Vec2 p2);
	float CubicBezier(float t, float p1, float p2, float p3, float p4);

	//-------------------------------------------------------------------------
	// 単位変換
	//-------------------------------------------------------------------------
	Vec3  HtoM(const Vec3& vec);
	float HtoM(float val);
	Vec3  MtoH(const Vec3& vec);
	float MtoH(float val);
}
