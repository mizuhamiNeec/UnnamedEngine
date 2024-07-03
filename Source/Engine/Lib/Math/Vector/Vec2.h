#pragma once

struct Vec2 final {
	float x;
	float y;

	Vec2() : x(0.0f), y(0.0f) {}

	Vec2(const float x, const float y) : x(x), y(y) {}

	/* ---------- 関数類 ---------- */
	float Length() const;
	float SqrLength() const;
	float Distance(const Vec2& other) const;
	float DotProduct(const Vec2& other) const;
	float CrossProduct(const Vec2& other) const;

	bool IsZero(float tolerance = 0.01f) const;

	Vec2 Normalize() const;
	Vec2 ClampLength(float min, float max) const;
	Vec2 RotateVector(float angle) const;
	Vec2 Lerp(const Vec2& end, float t) const;

	/* ---------- 演算子 ---------- */
	float& operator[](int index);
	const float& operator[](int index) const;

	Vec2 operator+(const Vec2& other) const;
	Vec2 operator-(const Vec2& other) const;
	Vec2 operator*(float scalar) const;
	Vec2 operator/(float divisor) const;

	Vec2 operator+(const float& scalar) const;
	Vec2 operator-(const float& scalar) const;
	Vec2 operator*(const Vec2& other) const;
	Vec2 operator/(const Vec2& other) const;

	Vec2& operator+=(const Vec2& other);
	Vec2& operator-=(const Vec2& other);
	Vec2& operator*=(float scalar);
	Vec2& operator/=(float divisor);
};