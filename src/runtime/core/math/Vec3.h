#pragma once
#include <cstdint>
#include <string>

struct Quaternion;
#include <runtime/core/math/Vec2.h>

struct Vec3 final {
	float x, y, z;

	static const Vec3 zero;
	static const Vec3 one;
	static const Vec3 right;
	static const Vec3 left;
	static const Vec3 up;
	static const Vec3 down;
	static const Vec3 forward;
	static const Vec3 backward;
	static const Vec3 max;
	static const Vec3 min;

	constexpr Vec3() : Vec3(0.0f, 0.0f, 0.0f) {
	}

	constexpr
	Vec3(const float x, const float y, const float z) : x(x), y(y), z(z) {
	}

	explicit constexpr Vec3(const float scalar) : x(scalar), y(scalar),
	                                              z(scalar) {
	}

	constexpr Vec3(const Vec2 vec2) : x(vec2.x), y(vec2.y), z(0.0f) {
	}

	/* ---------------- 関数類 ---------------- */
	float Length() const;
	float SqrLength() const;
	float Distance(const Vec3& other) const;
	float Dot(const Vec3& other) const;
	Vec3  Cross(const Vec3& other) const;

	bool IsZero(float tolerance = 1e-6f) const;
	bool IsParallel(const Vec3& other) const;

	void Normalize();
	Vec3 Normalized() const;

	Vec3 Clamp(Vec3 minVec, Vec3 maxVec) const;
	Vec3 ClampLength(float minVec, float maxVec);
	Vec3 Reflect(const Vec3& normal) const;

	Vec3 Abs();

	Vec3 TransformDirection(const Quaternion& rotation) const;

	/* ---------------- 演算子 ---------------- */
	float&       operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec3 operator-() const;

	Vec3 operator+(const Vec3& rhs) const;
	Vec3 operator-(const Vec3& rhs) const;
	Vec3 operator*(float rhs) const;
	Vec3 operator/(float rhs) const;

	Vec3 operator+(const float& rhs) const;
	Vec3 operator-(const float& rhs) const;
	Vec3 operator*(const Vec3& rhs) const;
	Vec3 operator/(const Vec3& rhs) const;

	friend Vec3 operator+(float lhs, const Vec3& rhs);
	friend Vec3 operator-(float lhs, const Vec3& rhs);
	friend Vec3 operator*(float lhs, const Vec3& rhs);
	friend Vec3 operator/(float lhs, const Vec3& rhs);

	Vec3& operator+=(const Vec3& rhs);
	Vec3& operator-=(const Vec3& rhs);
	Vec3& operator*=(float rhs);
	Vec3& operator/=(float rhs);

	/* ---------------- その他 ---------------- */
	std::string ToString() const;
	bool        operator!=(const Vec3& rhs) const;
	bool        operator==(const Vec3& vec3) const;

	static Vec3 Min(Vec3 lhs, Vec3 rhs);
	static Vec3 Max(Vec3 lhs, Vec3 rhs);
};
