#pragma once
#include <cstdint>

struct Vec2 final {
	float x, y;

	static const Vec2 zero;
	static const Vec2 one;
	static const Vec2 right;
	static const Vec2 left;
	static const Vec2 up;
	static const Vec2 down;

	constexpr Vec2(const float x = 0.0f, const float y = 0.0f) : x(x), y(y) {}

	/* ---------------- 関数類 ---------------- */
	float Length() const;
	float SqrLength() const;
	float Distance(const Vec2& other) const;
	float Dot(const Vec2& other) const;
	float Cross(const Vec2& other) const;

	bool IsZero(float tolerance = 1e-6f) const;

	void Normalize();
	Vec2 Normalized() const;

	Vec2 Clamp(Vec2 min, Vec2 max) const;
	Vec2 ClampLength(float min, float max);
	Vec2 Lerp(const Vec2& target, float t) const;
	Vec2 Reflect(const Vec2& normal) const;
	Vec2 RotateVector(float angleZ) const;

	/* ---------------- 演算子 ---------------- */
	float& operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec2 operator-() const;

	Vec2 operator+(const Vec2& rhs) const;
	Vec2 operator-(const Vec2& rhs) const;
	Vec2 operator*(float rhs) const;
	Vec2 operator/(float rhs) const;

	Vec2 operator+(const float& rhs) const;
	Vec2 operator-(const float& rhs) const;
	Vec2 operator*(const Vec2& rhs) const;
	Vec2 operator/(const Vec2& rhs) const;

	friend Vec2 operator+(float lhs, const Vec2& rhs);
	friend Vec2 operator-(float lhs, const Vec2& rhs);
	friend Vec2 operator*(float lhs, const Vec2& rhs);
	friend Vec2 operator/(float lhs, const Vec2& rhs);

	Vec2& operator+=(const Vec2& rhs);
	Vec2& operator-=(const Vec2& rhs);
	Vec2& operator*=(float rhs);
	Vec2& operator/=(float rhs);
};
