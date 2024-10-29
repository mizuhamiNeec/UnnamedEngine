#pragma once

#include "../Vector/Vec3.h"

struct Mat4 final {
	float m[4][4];

	Mat4 operator+(const Mat4& rhs) const;
	Mat4 operator-(const Mat4& rhs) const;
	Mat4 operator*(const Mat4& rhs) const;

	Mat4 Inverse() const;
	Mat4 Transpose() const;

	void LogMat4();

	static Mat4 Identity();
	static Mat4 Translate(const Vec3& translate);
	static Mat4 Scale(const Vec3& scale);

	static Vec3 Transform(const Vec3& vector, const Mat4& matrix);

	static Mat4 RotateX(float radian);
	static Mat4 RotateY(float radian);
	static Mat4 RotateZ(float radian);

	static Mat4 Affine(const Vec3& scale, const Vec3& rotate, const Vec3& translate);

	static Mat4 PerspectiveFovMat(float fovY, float aspectRatio, float nearClip, float farClip);
	static Mat4 MakeOrthographicMat(float left, float top, float right, float bottom, float nearClip, float farClip);
	static Mat4 FishEyeProjection(float fov, float aspect, float nearClip, float farClip);
	static Mat4 ViewportMat(float left, float top, float width, float height, float minDepth, float maxDepth);
};
