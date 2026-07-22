#include "AssimpConversions.h"

namespace Unnamed::AssimpConversions {
	Mat4 ToMat4(const aiMatrix4x4& matrix) {
		Mat4 out = Mat4::identity;
		out      = {
			{matrix.a1, matrix.b1, matrix.c1, matrix.d1},
			{matrix.a2, matrix.b2, matrix.c2, matrix.d2},
			{matrix.a3, matrix.b3, matrix.c3, matrix.d3},
			{matrix.a4, matrix.b4, matrix.c4, matrix.d4}
		};
		return out;
	}

	Vec3 ToVec3(const aiVector3D& vector) {
		return {vector.x, vector.y, vector.z};
	}

	Quaternion ToQuaternion(const aiQuaternion& quaternion) {
		return Quaternion(
			quaternion.x,
			quaternion.y,
			quaternion.z,
			quaternion.w
		).Normalized();
	}
}
