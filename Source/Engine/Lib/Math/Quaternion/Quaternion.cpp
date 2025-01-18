#include "Quaternion.h"

#include <algorithm>
#include <cmath>

#include "../MathLib.h"

const Quaternion Quaternion::identity = Quaternion(0, 0, 0, 1);

Quaternion::Quaternion() :
	x(0),
	y(0),
	z(0),
	w(1) {
}

Quaternion::Quaternion(const float x, const float y, const float z, const float w) :
	x(x),
	y(y),
	z(z),
	w(w) {
}

Quaternion::Quaternion(const Vec3& axis, const float angleRad) {
	const float halfAngle = angleRad * 0.5f;
	const float sinHalfAngle = std::sin(halfAngle);
	x = axis.x * sinHalfAngle;
	y = axis.y * sinHalfAngle;
	z = axis.z * sinHalfAngle;
	w = std::cos(halfAngle);
}

void Quaternion::Normalize() {
	const float len = std::sqrt(x * x + y * y + z * z + w * w);
	if (len > 0) {
		x /= len;
		y /= len;
		z /= len;
		w /= len;
	}
}

Quaternion Quaternion::Normalized() const {
	const float magnitude = std::sqrt(x * x + y * y + z * z + w * w);
	return { x / magnitude, y / magnitude, z / magnitude, w / magnitude };
}

Quaternion Quaternion::Conjugate() const {
	return { -x, -y, -z, w };
}

Quaternion Quaternion::Inverse() const {
	float normSquared = x * x + y * y + z * z + w * w;

	if (normSquared < 1e-6f) {
		return identity;
	}

	float invNormSquared = 1.0f / normSquared;
	return Quaternion(
		-x * invNormSquared,
		-y * invNormSquared,
		-z * invNormSquared,
		w * invNormSquared
	);
}

void Quaternion::ToAxisAngle(Vec3& outAxis, float& outAngle) const {
	const float scale = Vec3(x, y, z).SqrLength();
	if (scale > 1e-6) {
		outAxis = { x / scale, y / scale, z / scale };
		outAngle = 2.0f * std::acos(w);
	} else {
		outAxis = Vec3::right;
		outAngle = 0.0f;
	}
}

Quaternion Quaternion::Euler(const Vec3& eulerRad) {
	const float pitch = eulerRad.x; // X軸回転（ピッチ）
	const float yaw = eulerRad.y; // Y軸回転（ヨー）
	const float roll = eulerRad.z; // Z軸回転（ロール）

	const float sinPitch = std::sin(pitch * 0.5f);
	const float cosPitch = std::cos(pitch * 0.5f);
	const float sinYaw = std::sin(yaw * 0.5f);
	const float cosYaw = std::cos(yaw * 0.5f);
	const float sinRoll = std::sin(roll * 0.5f);
	const float cosRoll = std::cos(roll * 0.5f);

	return Quaternion(
		sinPitch * cosYaw * cosRoll - cosPitch * sinYaw * sinRoll,
		cosPitch * sinYaw * cosRoll + sinPitch * cosYaw * sinRoll,
		cosPitch * cosYaw * sinRoll - sinPitch * sinYaw * cosRoll,
		cosPitch * cosYaw * cosRoll + sinPitch * sinYaw * sinRoll
	).Normalized();
}

Quaternion Quaternion::Euler(const float& x, const float& y, const float& z) {
	return Euler({ x, y, z });
}

Quaternion Quaternion::EulerDegrees(const Vec3& eulerDeg) {
	return EulerDegrees(eulerDeg.x, eulerDeg.y, eulerDeg.z);
}

Quaternion Quaternion::EulerDegrees(const float& x, const float& y, const float& z) {
	return Euler(x * Math::deg2Rad, y * Math::deg2Rad, z * Math::deg2Rad);
}

Quaternion Quaternion::AxisAngle(const Vec3& axis, const float& angleDeg) {
	const float angleRad = angleDeg;
	return Quaternion(axis, angleRad);
}

Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	Quaternion result = {
		(1.0f - t) * a.x + t * b.x,
		(1.0f - t) * a.y + t * b.y,
		(1.0f - t) * a.z + t * b.z,
		(1.0f - t) * a.w + t * b.w
	};
	return result.Normalized();
}

Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
	t = std::clamp(t, 0.0f, 1.0f);

	// a と b の内積を計算
	float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

	// dot が 1 または -1 になる可能性を防ぐ（数値誤差を避ける）
	dot = std::clamp(dot, -1.0f, 1.0f);

	// angle を計算
	float angle = std::acos(dot);

	// angle が非常に小さい場合は Lerp に切り替え
	if (angle < 1e-6f) {
		return a;  // angle が 0 に近い場合、Slerp の結果は a とほぼ同じ
	}

	// sin(angle) を計算
	float sinAngle = std::sin(angle);

	// sinAngle が非常に小さい場合は Lerp に切り替え
	if (std::abs(sinAngle) < 1e-6f) {
		return Lerp(a, b, t);  // Lerp を使用して補完
	}

	// t が 0 または 1 の場合は早期リターン
	if (t == 0.0f) {
		return a;
	}
	if (t == 1.0f) {
		return b;
	}

	// Slerp の計算
	float scale0 = std::sin((1.0f - t) * angle) / sinAngle;
	float scale1 = std::sin(t * angle) / sinAngle;

	// dot が負の場合、反転する
	if (dot < 0.0f) {
		scale1 = -scale1;
	}

	// 最終的な Quaternion を計算し、正規化
	return Quaternion(
		scale0 * a.x + scale1 * b.x,
		scale0 * a.y + scale1 * b.y,
		scale0 * a.z + scale1 * b.z,
		scale0 * a.w + scale1 * b.w
	).Normalized();  // 結果を正規化して返す
}

Vec3 Quaternion::ToEulerAngles() const {
	Vec3 euler;

	// Roll
	const float sinRCosP = 2 * (w * x + y * z);
	const float cosRCosP = 1 - 2 * (x * x + y * y);
	euler.x = std::atan2(sinRCosP, cosRCosP);

	// Pitch
	const float sinP = 2 * (w * y - z * x);
	if (std::abs(sinP) >= 1) {
		euler.y = std::copysign(Math::pi * 0.5f, sinP);
	} else {
		euler.y = std::asin(sinP);
	}

	// Yaw
	const float sinYCosP = 2 * (w * z + x * y);
	const float cosYCosP = 1 - 2 * (y * y + z * z);
	euler.z = std::atan2(sinYCosP, cosYCosP);

	return euler;
}

Vec3 Quaternion::ToEulerDegrees() {
	return ToEulerAngles() * Math::rad2Deg;
}

Vec3 Quaternion::GetAxis() const {
	const float scale = std::sqrt(1.0f - w * w);
	if (scale < 1e-6f) {
		return Vec3::up;
	}
	return { x / scale, y / scale, z / scale };
}

Vec3 Quaternion::RotateVector(Vec3 vec3) {
	Quaternion qVec(vec3.x, vec3.y, vec3.z, 0.0f);
	Quaternion qConj = Conjugate();
	Quaternion qResult = *this * qVec * qConj;
	return { qResult.x, qResult.y, qResult.z };
}

float Quaternion::GetRotationAroundAxis(const Vec3& axis) const {
	Vec3 normalizedAxis = axis.Normalized();

	Vec3 quaternionAxis;
	float angle;
	ToAxisAngle(quaternionAxis, angle);

	float projection = quaternionAxis.Dot(normalizedAxis);

	return angle * projection;
}

float Quaternion::GetAngle() const {
	return 2.0f * std::acos(std::clamp(w, -1.0f, 1.0f));
}

float Quaternion::GetAngleDegrees() const {
	return GetAngle() * Math::rad2Deg;
}

Quaternion Quaternion::operator*(const Quaternion& other) const {
	return {
		w * other.x + x * other.w + y * other.z - z * other.y,
		w * other.y - x * other.z + y * other.w + z * other.x,
		w * other.z + x * other.y - y * other.x + z * other.w,
		w * other.w - x * other.x - y * other.y - z * other.z
	};
}

Vec3 Quaternion::operator*(const Vec3& vec) const {
	const float tx = 2.0f * x;
	const float ty = 2.0f * y;
	const float tz = 2.0f * z;
	const float twx = tx * w;
	const float twy = ty * w;
	const float twz = tz * w;
	const float txx = tx * x;
	const float txy = ty * x;
	const float txz = tz * x;
	const float tyy = ty * y;
	const float tyz = tz * y;
	const float tzz = tz * z;

	return Vec3(
		vec.x * (1.0f - (tyy + tzz)) + vec.y * (txy - twz) + vec.z * (txz + twy),
		vec.x * (txy + twz) + vec.y * (1.0f - (txx + tzz)) + vec.z * (tyz - twx),
		vec.x * (txz - twy) + vec.y * (tyz + twx) + vec.z * (1.0f - (txx + tyy))
	);
}
