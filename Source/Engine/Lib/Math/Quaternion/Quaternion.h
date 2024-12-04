#pragma once

#include "../Lib/Math/Vector/Vec3.h"

struct Quaternion {
	float x, y, z, w;

	static const Quaternion identity;

	Quaternion();
	Quaternion(float x, float y, float z, float w);
	Quaternion(const Vec3& axis, float angleRad);

	//-------------------------------------------------------------------------
	// Func
	//-------------------------------------------------------------------------
	void Normalize();
	Quaternion Normalized() const;
	Quaternion Conjugate() const;
	void ToAxisAngle(Vec3& outAxis, float& outAngle) const;
	static Quaternion Euler(const Vec3& eulerRad);
	static Quaternion Euler(const float& x, const float& y, const float& z);
	static Quaternion AngleAxis(const float& angleDeg, const Vec3& axis);
	static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
	static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
	Vec3 ToEulerAngles() const;

	//-------------------------------------------------------------------------
	// Operator
	//-------------------------------------------------------------------------
	Quaternion operator*(const Quaternion& other) const;
	Vec3 operator*(const Vec3& vec) const;
};
