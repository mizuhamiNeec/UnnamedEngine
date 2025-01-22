#pragma once

#include "Lib/Math/Vector/Vec3.h"

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
	Quaternion Inverse() const;
	void ToAxisAngle(Vec3& outAxis, float& outAngle) const;
	static Quaternion Euler(const Vec3& eulerRad);
	static Quaternion Euler(const float& x, const float& y, const float& z);
	static Quaternion EulerDegrees(const Vec3& eulerDeg);
	static Quaternion EulerDegrees(const float& x, const float& y, const float& z);
	static Quaternion AxisAngle(const Vec3& axis, const float& angleDeg);
	static Quaternion LookRotation(const Vec3& forward, const Vec3& up = Vec3::up);
	static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
	static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
	Vec3 ToEulerAngles() const;
	Vec3 ToEulerDegrees();
	Vec3 GetAxis() const;
	Vec3 RotateVector(Vec3 vec3);
	float GetRotationAroundAxis(const Vec3& axis) const;
	float GetAngle() const;
	float GetAngleDegrees() const;

	//-------------------------------------------------------------------------
	// Operator
	//-------------------------------------------------------------------------
	Quaternion operator*(const Quaternion& other) const;
	Vec3 operator*(const Vec3& vec) const;
};
