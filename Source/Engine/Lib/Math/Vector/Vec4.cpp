#include "Vec4.h"

#include <Lib/Math/Matrix/Mat4.h>

Vec4 Vec4::one = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
Vec4 Vec4::zero = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

Vec4 Vec4::red = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
Vec4 Vec4::green = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
Vec4 Vec4::blue = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
Vec4 Vec4::white = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
Vec4 Vec4::black = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
Vec4 Vec4::yellow = Vec4(1.0f, 1.0f, 0.0f, 1.0f);
Vec4 Vec4::cyan = Vec4(0.0f, 1.0f, 1.0f, 1.0f);
Vec4 Vec4::magenta = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
Vec4 Vec4::gray = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
Vec4 Vec4::lightGray = Vec4(0.75f, 0.75f, 0.75f, 1.0f);
Vec4 Vec4::darkGray = Vec4(0.25f, 0.25f, 0.25f, 1.0f);
Vec4 Vec4::orange = Vec4(1.0f, 0.5f, 0.0f, 1.0f);
Vec4 Vec4::purple = Vec4(0.5f, 0.0f, 0.5f, 1.0f);
Vec4 Vec4::brown = Vec4(0.6f, 0.3f, 0.0f, 1.0f);

Vec4 Vec4::operator*(const Mat4& mat4) const {
	return Vec4(
		x * mat4.m[0][0] + y * mat4.m[1][0] + z * mat4.m[2][0] + w * mat4.m[3][0],
		x * mat4.m[0][1] + y * mat4.m[1][1] + z * mat4.m[2][1] + w * mat4.m[3][1],
		x * mat4.m[0][2] + y * mat4.m[1][2] + z * mat4.m[2][2] + w * mat4.m[3][2],
		x * mat4.m[0][3] + y * mat4.m[1][3] + z * mat4.m[2][3] + w * mat4.m[3][3]
	);
}

Vec4 Vec4::operator*(const float rhs) const {
	return Vec4(x * rhs, y * rhs, z * rhs, w * rhs);
}

Vec4 Vec4::operator+(const Vec4& vec4) const {
	return Vec4(x + vec4.x, y + vec4.y, z + vec4.z, w + vec4.w);
}

#ifdef _DEBUG
ImVec4 ToImVec4(const Vec4& vec) {
	return { vec.x, vec.y, vec.z, vec.w };
}
#endif
