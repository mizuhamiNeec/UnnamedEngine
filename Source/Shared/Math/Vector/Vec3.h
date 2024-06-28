#pragma once

struct Vec3 final {
	float x, y, z;

	Vec3();
	Vec3(const float& x, const float& y, const float& z);

	static Vec3 Zero();
	static Vec3 Forward();
	static Vec3 Up();
	static Vec3 Right();

	//------------------------------------------------------------------------
	// 関数
	//------------------------------------------------------------------------

	float Length() const;
	float SqrLength() const;
	float Distance(const Vec3& other) const;
	float DotProduct(const Vec3& other) const;
	Vec3 CrossProduct(const Vec3& other) const;
	Vec3 Normalize() const;
	//Vec3 Lerp(const Vec3& start, const Vec3& end, float t) const;

	//------------------------------------------------------------------------
	// 演算子
	//------------------------------------------------------------------------

	float& operator[](int index);
	const float& operator[](int index) const;

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
	Vec3& operator*=(const float& rhs);
	Vec3& operator/=(const float& rhs);
};
