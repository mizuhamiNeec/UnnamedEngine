#pragma once
#include <cstdint>
#include <string>

struct Vec2;

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

	Vec3(float x = 0.0f, float y = 0.0f, float z = 0.0f);
	Vec3(Vec2 vec2);

	/* ---------------- 関数類 ---------------- */
	float Length() const;
	float SqrLength() const;
	float Distance(const Vec3& other) const;
	float Dot(const Vec3& other) const;
	Vec3 Cross(const Vec3& other) const;

	bool IsZero(float tolerance = 1e-6f) const;
	bool IsParallel(const Vec3& other) const;

	void Normalize();
	Vec3 Normalized() const;

	Vec3 Clamp(Vec3 min, Vec3 max) const;
	Vec3 ClampLength(float min, float max);
	Vec3 Reflect(const Vec3& normal) const;

	/* ---------------- 演算子 ---------------- */
	float& operator[](uint32_t index);
	const float& operator[](uint32_t index) const;


	Vec3 operator-() const {
		return { -x, -y, -z };
	}

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
	std::string ToString() const;
};
