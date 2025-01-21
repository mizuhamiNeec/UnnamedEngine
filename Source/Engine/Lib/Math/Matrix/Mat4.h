#pragma once

#include <initializer_list>

#include "../Quaternion/Quaternion.h"

#include "../Vector/Vec3.h"
#include "Lib/Math/Vector/Vec4.h"

struct Mat4 final {
	float m[4][4];

	Mat4();
	Mat4(const Mat4& other);
	Mat4(std::initializer_list<std::initializer_list<float>> list);

	static const Mat4 identity;
	static const Mat4 zero;

	//-------------------------------------------------------------------------
	// Functions
	//-------------------------------------------------------------------------
	[[nodiscard]] Mat4 Inverse() const;
	[[nodiscard]] Mat4 Transpose() const;

	void LogMat4(const std::string& matName);

	static Mat4 Translate(const Vec3& translate);
	static Mat4 Scale(const Vec3& scale);
	static Vec3 Transform(const Vec3& vector, const Mat4& matrix);
	static Mat4 RotateQuaternion(Quaternion quaternion);
	static Mat4 FromQuaternion(const Quaternion& q);
	static Mat4 RotateX(float radian);
	static Mat4 RotateY(float radian);
	static Mat4 RotateZ(float radian);
	static Mat4 Affine(const Vec3& scale, const Vec3& rotate, const Vec3& translate);
	static Mat4 PerspectiveFovMat(float fovY, float aspectRatio, float nearClip, float farClip);
	static Mat4 MakeOrthographicMat(float left, float top, float right, float bottom, float nearClip, float farClip);
	static Mat4 ViewportMat(float left, float top, float width, float height, float minDepth, float maxDepth);

	Quaternion ToQuaternion() const;

	Vec3& TransformPoint(Vec3 vec3) const;

	Vec3 GetTranslate();
	Vec3 GetRotate() const;
	Vec3 GetRight();
	Vec3 GetUp();
	Vec3 GetForward();

	//-------------------------------------------------------------------------
	// Operator
	//-------------------------------------------------------------------------
	Mat4 operator+(const Mat4& rhs) const;
	Mat4 operator-(const Mat4& rhs) const;
	Mat4 operator*(const Mat4& rhs) const;
	Vec4 operator*(const Vec4& vec) const;

	Mat4& operator*=(const Mat4& mat4);

private:
	float Determinant() const;
};
