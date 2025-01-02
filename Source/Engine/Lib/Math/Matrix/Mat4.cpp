#include "Mat4.h"

#include <cassert>
#include <cmath>
#include <format>

#include "../../Console/Console.h"
#include "../Lib/Utils/StrUtils.h"
#include "../MathLib.h"

Mat4::Mat4() {
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			m[i][j] = (i == j) ? 1.0f : 0.0f;
		}
	}
}

Mat4::Mat4(const Mat4& other) {
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			m[i][j] = other.m[i][j];
		}
	}
}

Mat4::Mat4(const std::initializer_list<std::initializer_list<float>> list) {
	int row = 0;
	for (const auto& sublist : list) {
		int col = 0;
		for (const float value : sublist) {
			if (row < 4 && col < 4) {
				m[row][col] = value;
			}
			++col;
		}
		++row;
	}

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			if (i >= static_cast<int>(list.size()) || j >= static_cast<int>(((list.begin() + i))->size())) {
				m[i][j] = 0.0f;
			}
		}
	}
}

const Mat4 Mat4::identity = Mat4();
const Mat4 Mat4::zero = {{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}};

Mat4 Mat4::operator+(const Mat4& rhs) const {
	return {
		{
			{m[0][0] + rhs.m[0][0], m[0][1] + rhs.m[0][1], m[0][2] + rhs.m[0][2], m[0][3] + rhs.m[0][3]},
			{m[1][0] + rhs.m[1][0], m[1][1] + rhs.m[1][1], m[1][2] + rhs.m[1][2], m[1][3] + rhs.m[1][3]},
			{m[2][0] + rhs.m[2][0], m[2][1] + rhs.m[2][1], m[2][2] + rhs.m[2][2], m[2][3] + rhs.m[2][3]},
			{m[3][0] + rhs.m[3][0], m[3][1] + rhs.m[3][1], m[3][2] + rhs.m[3][2], m[3][3] + rhs.m[3][3]},
		}};
}

Mat4 Mat4::operator-(const Mat4& rhs) const {
	return {
		{
			{m[0][0] - rhs.m[0][0], m[0][1] - rhs.m[0][1], m[0][2] - rhs.m[0][2], m[0][3] - rhs.m[0][3]},
			{m[1][0] - rhs.m[1][0], m[1][1] - rhs.m[1][1], m[1][2] - rhs.m[1][2], m[1][3] - rhs.m[1][3]},
			{m[2][0] - rhs.m[2][0], m[2][1] - rhs.m[2][1], m[2][2] - rhs.m[2][2], m[2][3] - rhs.m[2][3]},
			{m[3][0] - rhs.m[3][0], m[3][1] - rhs.m[3][1], m[3][2] - rhs.m[3][2], m[3][3] - rhs.m[3][3]},
		},
	};
}

Mat4 Mat4::operator*(const Mat4& rhs) const {
	Mat4 result;
	float rhsColumn[4][4];
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			rhsColumn[i][j] = rhs.m[j][i];
		}
	}

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.m[row][col] = m[row][0] * rhsColumn[col][0] +
				m[row][1] * rhsColumn[col][1] +
				m[row][2] * rhsColumn[col][2] +
				m[row][3] * rhsColumn[col][3];
		}
	}
	return result;
}

Mat4& Mat4::operator*=(const Mat4& mat4) {
	*this = *this * mat4;
	return *this;
}

float Mat4::Determinant() const {
	return m[0][0] * (m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) + m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])) - m[0][1] * (m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])) + m[0][2] * (m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) - m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])) - m[0][3] * (m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) - m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) + m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
}

Mat4 Mat4::Inverse() const {
	Mat4 result;
	const float det = Determinant();

	if (det == 0.0f) {
		Console::Print("Mat4 : 行列式がゼロのため、逆行列は存在しません。\n", kConsoleColorError);
		return result;
	}

	// 逆行列の計算
	const float invDet = 1.0f / det;

	result.m[0][0] = invDet * (m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) + m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]));
	result.m[0][1] = invDet * -(m[0][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[0][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) + m[0][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]));
	result.m[0][2] = invDet * (m[0][1] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) - m[0][2] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) + m[0][3] * (m[1][1] * m[3][2] - m[1][2] * m[3][1]));
	result.m[0][3] = invDet * -(m[0][1] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) - m[0][2] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) + m[0][3] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]));

	result.m[1][0] = invDet * -(m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]));
	result.m[1][1] = invDet * (m[0][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) - m[0][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[0][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]));
	result.m[1][2] = invDet * -(m[0][0] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) - m[0][2] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) + m[0][3] * (m[1][0] * m[3][2] - m[1][2] * m[3][0]));
	result.m[1][3] = invDet * (m[0][0] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) - m[0][2] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) + m[0][3] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]));

	result.m[2][0] = invDet * (m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) - m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	result.m[2][1] = invDet * -(m[0][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) - m[0][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) + m[0][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	result.m[2][2] = invDet * (m[0][0] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) - m[0][1] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) + m[0][3] * (m[1][0] * m[3][1] - m[1][1] * m[3][0]));
	result.m[2][3] = invDet * -(m[0][0] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) - m[0][1] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) + m[0][3] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));

	result.m[3][0] = invDet * -(m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) - m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) + m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	result.m[3][1] = invDet * (m[0][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) - m[0][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) + m[0][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	result.m[3][2] = invDet * -(m[0][0] * (m[1][1] * m[3][2] - m[1][2] * m[3][1]) - m[0][1] * (m[1][0] * m[3][2] - m[1][2] * m[3][0]) + m[0][2] * (m[1][0] * m[3][1] - m[1][1] * m[3][0]));
	result.m[3][3] = invDet * (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));

	return result;
}

Mat4 Mat4::Transpose() const {
	return {
		{{m[0][0], m[1][0], m[2][0], m[3][0]},
		 {m[0][1], m[1][1], m[2][1], m[3][1]},
		 {m[0][2], m[1][2], m[2][2], m[3][2]},
		 {m[0][3], m[1][3], m[2][3], m[3][3]}}};
}

void Mat4::LogMat4() {
	std::wstring result;
	for (float(&i)[4] : m) {
		for (float& j : i) {
			result += std::format(L"{:.2f} ", j);
		}
		result += L"\n";
	}
	Console::Print(StrUtils::ToString(result));
}

Mat4 Mat4::Translate(const Vec3& translate) {
	return {
		{{1.0f, 0.0f, 0.0f, 0.0f},
		 {0.0f, 1.0f, 0.0f, 0.0f},
		 {0.0f, 0.0f, 1.0f, 0.0f},
		 {translate.x, translate.y, translate.z, 1.0f}},
	};
}

Mat4 Mat4::Scale(const Vec3& scale) {
	return {
		{{scale.x, 0.0f, 0.0f, 0.0f},
		 {0.0f, scale.y, 0.0f, 0.0f},
		 {0.0f, 0.0f, scale.z, 0.0f},
		 {0.0f, 0.0f, 0.0f, 1.0f}},
	};
}

Vec3 Mat4::Transform(const Vec3& vector, const Mat4& matrix) {
	Vec3 result; // w=1がデカルト座標系であるので(x,y,z,1)のベクトルとしてmatrixとの積をとる
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];
	assert(w != 0.0f); // ベクトルに対して基本的な操作を行う行列でwが0になることはありえない
	result.x /= w; // w=1がデカルト座標系であるので、w除算することで同時座標をデカルト座標に戻す
	result.y /= w;
	result.z /= w;
	return result;
}

Mat4 Mat4::RotateQuaternion(const Quaternion quaternion) {
	Mat4 result = identity;
	const auto normalizedQuaternion = quaternion.Normalized();

	const float xx = normalizedQuaternion.x * normalizedQuaternion.x;
	const float yy = normalizedQuaternion.y * normalizedQuaternion.y;
	const float zz = normalizedQuaternion.z * normalizedQuaternion.z;
	const float xy = normalizedQuaternion.x * normalizedQuaternion.y;
	const float xz = normalizedQuaternion.x * normalizedQuaternion.z;
	const float yz = normalizedQuaternion.y * normalizedQuaternion.z;
	const float wx = normalizedQuaternion.w * normalizedQuaternion.x;
	const float wy = normalizedQuaternion.w * normalizedQuaternion.y;
	const float wz = normalizedQuaternion.w * normalizedQuaternion.z;

	result.m[0][0] = 1.0f - 2.0f * (yy + zz);
	result.m[0][1] = 2.0f * (xy - wz);
	result.m[0][2] = 2.0f * (xz + wy);

	result.m[1][0] = 2.0f * (xy + wz);
	result.m[1][1] = 1.0f - 2.0f * (xx + zz);
	result.m[1][2] = 2.0f * (yz - wx);

	result.m[2][0] = 2.0f * (xz - wy);
	result.m[2][1] = 2.0f * (yz + wx);
	result.m[2][2] = 1.0f - 2.0f * (xx + yy);

	return result;
}

Mat4 Mat4::FromQuaternion(const Quaternion& q) {
	Mat4 result;

	float xx = q.x * q.x;
	float yy = q.y * q.y;
	float zz = q.z * q.z;
	float xy = q.x * q.y;
	float xz = q.x * q.z;
	float yz = q.y * q.z;
	float wx = q.w * q.x;
	float wy = q.w * q.y;
	float wz = q.w * q.z;

	result.m[0][0] = 1.0f - 2.0f * (yy + zz);
	result.m[0][1] = 2.0f * (xy - wz);
	result.m[0][2] = 2.0f * (xz + wy);
	result.m[0][3] = 0.0f;

	result.m[1][0] = 2.0f * (xy + wz);
	result.m[1][1] = 1.0f - 2.0f * (xx + zz);
	result.m[1][2] = 2.0f * (yz - wx);
	result.m[1][3] = 0.0f;

	result.m[2][0] = 2.0f * (xz - wy);
	result.m[2][1] = 2.0f * (yz + wx);
	result.m[2][2] = 1.0f - 2.0f * (xx + yy);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Mat4 Mat4::RotateX(const float radian) {
	Mat4 result = identity;

	const float cos = std::cos(radian);
	const float sin = std::sin(radian);

	result.m[1][1] = cos;
	result.m[1][2] = sin;
	result.m[2][1] = -sin;
	result.m[2][2] = cos;

	return result;
}

Mat4 Mat4::RotateY(const float radian) {
	Mat4 result = identity;

	const float cos = std::cos(radian);
	const float sin = std::sin(radian);

	result.m[0][0] = cos;
	result.m[0][2] = -sin;
	result.m[2][0] = sin;
	result.m[2][2] = cos;

	return result;
}

Mat4 Mat4::RotateZ(const float radian) {
	Mat4 result = identity;

	const float cos = std::cos(radian);
	const float sin = std::sin(radian);

	result.m[0][0] = cos;
	result.m[0][1] = sin;
	result.m[1][0] = -sin;
	result.m[1][1] = cos;

	return result;
}

Mat4 Mat4::Affine(const Vec3& scale, const Vec3& rotate, const Vec3& translate) {
	const Mat4 s = Scale(scale);
	const Mat4 rx = RotateX(rotate.x);
	const Mat4 ry = RotateY(rotate.y);
	const Mat4 rz = RotateZ(rotate.z);
	const Mat4 t = Translate(translate);

	return s * rx * ry * rz * t;
}

Mat4 Mat4::PerspectiveFovMat(const float fovY, const float aspectRatio, const float nearClip, const float farClip) {
	Mat4 result = identity;

	const float cot = 1.0f / std::tan(fovY * 0.5f);
	const float dist = farClip - nearClip;

	result.m[0][0] = cot / aspectRatio;
	result.m[1][1] = cot;
	result.m[2][2] = (farClip + nearClip) / dist;
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / dist;
	result.m[3][3] = 0.0f;

	return result;
}

Mat4 Mat4::MakeOrthographicMat(
	const float left, const float top, const float right, const float bottom,
	const float nearClip, const float farClip) {
	Mat4 result = identity;

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);

	return result;
}

Mat4 Mat4::ViewportMat(
	const float left, const float top, const float width, const float height, const float minDepth,
	const float maxDepth) {
	Mat4 result = identity;

	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;

	return result;
}

Quaternion Mat4::ToQuaternion() const {
	Quaternion q;
	float trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0) {
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (m[2][1] - m[1][2]) * s;
		q.y = (m[0][2] - m[2][0]) * s;
		q.z = (m[1][0] - m[0][1]) * s;
	} else {
		if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
			float s = 2.0f * sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]);
			q.w = (m[2][1] - m[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (m[0][1] + m[1][0]) / s;
			q.z = (m[0][2] + m[2][0]) / s;
		} else if (m[1][1] > m[2][2]) {
			float s = 2.0f * sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]);
			q.w = (m[0][2] - m[2][0]) / s;
			q.x = (m[0][1] + m[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (m[1][2] + m[2][1]) / s;
		} else {
			float s = 2.0f * sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]);
			q.w = (m[1][0] - m[0][1]) / s;
			q.x = (m[0][2] + m[2][0]) / s;
			q.y = (m[1][2] + m[2][1]) / s;
			q.z = 0.25f * s;
		}
	}
	return q;
}

Vec3 Mat4::GetTranslate() {
	return {m[3][0], m[3][1], m[3][2]};
}
