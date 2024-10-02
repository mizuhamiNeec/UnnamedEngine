#pragma once
#include <cstdint>

struct Vec3 final {
	float x, y, z;

	static const Vec3 zero;
	static const Vec3 right;
	static const Vec3 left;
	static const Vec3 up;
	static const Vec3 down;
	static const Vec3 forward;
	static const Vec3 backward;

	Vec3() : x(0.0f), y(0.0f), z(0) {}
	Vec3(const float x, const float y, const float z) : x(x), y(y), z(z) {}
	Vec3(const Vec3& other) : x(other.x), y(other.y), z(other.z) {}

	/* ---------------- 関数類 ---------------- */
	float Length() const;
	float SqrLength() const;
	float Distance(const Vec3& other) const;
	float Dot(const Vec3& other) const;
	Vec3 Cross(const Vec3& other) const;

	bool IsZero(float tolerance = 0.01f) const;

	void Normalize();
	Vec3 Normalized() const;

	Vec3 Clamp(Vec3 min, Vec3 max) const;
	Vec3 ClampLength(float min, float max);
	Vec3 Lerp(const Vec3& target, float t) const;
	Vec3 Reflect(const Vec3& normal) const;
	Vec3 RotateVector(float angleZ) const;

	/* ---------------- 演算子 ---------------- */
	float& operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec3 operator+(const Vec3& rhs) const;
	Vec3 operator-(const Vec3& rhs) const;
	Vec3 operator*(float rhs) const;
	Vec3 operator/(float rhs) const;

	Vec3 operator+(const float& rhs) const;
	Vec3 operator-(const float& rhs) const;
	Vec3 operator*(const Vec3& rhs) const;
	Vec3 operator/(const Vec3& rhs) const;

	Vec3& operator+=(const Vec3& rhs);
	Vec3& operator-=(const Vec3& rhs);
	Vec3& operator*=(float rhs);
	Vec3& operator/=(float rhs);
};
