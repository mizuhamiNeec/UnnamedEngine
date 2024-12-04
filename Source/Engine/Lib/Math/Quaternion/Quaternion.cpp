#include "Quaternion.h"

#include <algorithm>
#include <cmath>

#include "../MathLib.h"

const Quaternion Quaternion::identity = Quaternion(0, 0, 0, 1);

Quaternion::Quaternion(): x(0),
                          y(0),
                          z(0),
                          w(1) {}

Quaternion::Quaternion(const float x, const float y, const float z, const float w): x(x),
	y(y),
	z(z),
	w(w) {}

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
	return {x / magnitude, y / magnitude, z / magnitude, w / magnitude};
}

Quaternion Quaternion::Conjugate() const {
	return {-x, -y, -z, -w};
}

void Quaternion::ToAxisAngle(Vec3& outAxis, float& outAngle) const {
	const float scale = Vec3(x, y, z).SqrLength();
	if (scale > 1e-6) {
		outAxis = {x / scale, y / scale, z / scale};
		outAngle = 2.0f * std::acos(w);
	} else {
		outAxis = Vec3::right;
		outAngle = 0.0f;
	}
}

Quaternion Quaternion::Euler(const Vec3& eulerRad) {
	const float roll = eulerRad.x; // X軸回転（ピッチ）
	const float pitch = eulerRad.y; // Y軸回転（ヨー）
	const float yaw = eulerRad.z; // Z軸回転（ロール）

	const float sinRoll = std::sin(roll * 0.5f);
	const float cosRoll = std::cos(roll * 0.5f);
	const float sinPitch = std::sin(pitch * 0.5f);
	const float cosPitch = std::cos(pitch * 0.5f);
	const float sinYaw = std::sin(yaw * 0.5f);
	const float cosYaw = std::cos(yaw * 0.5f);

	return Quaternion(
		sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
		cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
		cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
		cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw
	).Normalized();
}

Quaternion Quaternion::Euler(const float& x, const float& y, const float& z) {
	return Euler({x, y, z});
}

Quaternion Quaternion::AngleAxis(const float& angleDeg, const Vec3& axis) {
	const float angleRad = angleDeg * Math::deg2Rad;
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

Quaternion Quaternion::Slerp(
	[[maybe_unused]] const Quaternion& a, [[maybe_unused]] const Quaternion& b,
	[[maybe_unused]] float t
) {
	return identity;
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

Quaternion Quaternion::operator*(const Quaternion& other) const {
	return {
		w * other.x + x * other.w + y * other.z - z * other.y,
		w * other.y - x * other.z + y * other.w + z * other.x,
		w * other.z + x * other.y - y * other.x + z * other.w,
		w * other.w - x * other.x - y * other.y - z * other.z
	};
}

Vec3 Quaternion::operator*(const Vec3& vec) const {
	// より直接的な回転変換の実装
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
