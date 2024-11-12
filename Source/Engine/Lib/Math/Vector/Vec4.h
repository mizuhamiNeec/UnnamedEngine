#pragma once

#include "../Lib/Math/Matrix/Mat4.h"

struct Vec4 final {
	float x, y, z, w;

	Vec4 operator*(const Mat4& mat) const {
		return Vec4(
			x * mat.m[0][0] + y * mat.m[1][0] + z * mat.m[2][0] + w * mat.m[3][0],
			x * mat.m[0][1] + y * mat.m[1][1] + z * mat.m[2][1] + w * mat.m[3][1],
			x * mat.m[0][2] + y * mat.m[1][2] + z * mat.m[2][2] + w * mat.m[3][2],
			x * mat.m[0][3] + y * mat.m[1][3] + z * mat.m[2][3] + w * mat.m[3][3]
		);
	}

	Vec4& operator/=(float scalar) {
		if (scalar != 0) {
			x /= scalar;
			y /= scalar;
			z /= scalar;
			w /= scalar;
		}
		return *this;
	}
};
