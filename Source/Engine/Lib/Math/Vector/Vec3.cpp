#include "Vec3.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "Vec2.h"

const Vec3 Vec3::zero(0.0f, 0.0f, 0.0f);
const Vec3 Vec3::one(1.0f, 1.0f, 1.0f);
const Vec3 Vec3::right(1.0f, 0.0f, 0.0f);
const Vec3 Vec3::left(-1.0f, 0.0f, 0.0f);
const Vec3 Vec3::up(0.0f, 1.0f, 0.0f);
const Vec3 Vec3::down(0.0f, -1.0f, 0.0f);
const Vec3 Vec3::forward(0.0f, 0.0f, 1.0f);
const Vec3 Vec3::backward(0.0f, 0.0f, -1.0f);

Vec3::Vec3(const float x, const float y, const float z) : x(x), y(y), z(z) {
}

Vec3::Vec3(const Vec2 vec2) : x(vec2.x), y(vec2.y), z(0.0f) {
}

float Vec3::Length() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		return std::sqrt(sqrLength);
	}
	return 0.0f;
}

float Vec3::SqrLength() const {
	return x * x + y * y + z * z;
}

float Vec3::Distance(const Vec3& other) const {
	const float distX = other.x - x;
	const float distY = other.y - y;
	const float distZ = other.z - z;
	return std::sqrt(distX * distX + distY * distY + distZ * distZ);
}

float Vec3::Dot(const Vec3& other) const {
	return x * other.x + y * other.y + z * other.z;
}

Vec3 Vec3::Cross(const Vec3& other) const {
	return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
}

bool Vec3::IsZero(const float tolerance) const {
	return std::fabs(x) < tolerance && std::fabs(y) < tolerance;
}

void Vec3::Normalize() {
	const float len = Length();
	if (len > 0) {
		x /= len;
		y /= len;
		z /= len;
	}
}

Vec3 Vec3::Normalized() const {
	const float len = Length();
	if (len > 0) {
		return {x / len, y / len, z / len};
	}
	return zero;
}

Vec3 Vec3::Clamp(const Vec3 min, const Vec3 max) const {
	return {
		std::clamp(x, min.x, max.x),
		std::clamp(y, min.y, max.y),
		std::clamp(z, min.z, max.z)
	};
}

Vec3 Vec3::ClampLength(const float min, const float max) {
	const float sqrLength = SqrLength();
	if (sqrLength > max * max) {
		const float scale = max / std::sqrt(sqrLength);
		return {x * scale, y * scale, z * scale};
	}
	if (sqrLength < min * min) {
		const float scale = min / std::sqrt(sqrLength);
		return {x * scale, y * scale, z * scale};
	}
	return {x, y, z};
}

Vec3 Vec3::Lerp(const Vec3& target, float t) const {
	return *this * (1 - t) + target * t;
}

Vec3 Vec3::Reflect(const Vec3& normal) const {
	return *this - 2 * this->Dot(normal) * normal;
}

float& Vec3::operator[](const uint32_t index) {
	switch (index) {
	case 0: return x;
	case 1: return y;
	case 2: return z;
	default: throw std::out_of_range("Vec3 添字演算子");
	}
}

const float& Vec3::operator[](const uint32_t index) const {
	switch (index) {
	case 0: return x;
	case 1: return y;
	case 2: return z;
	default: throw std::out_of_range("Vec3 添字演算子");
	}
}

Vec3 Vec3::operator+(const Vec3& rhs) const {
	return {x + rhs.x, y + rhs.y, z + rhs.z};
}

Vec3 Vec3::operator-(const Vec3& rhs) const {
	return {x - rhs.x, y - rhs.y, z - rhs.z};
}

Vec3 Vec3::operator*(const float rhs) const {
	return {x * rhs, y * rhs, z * rhs};
}

Vec3 Vec3::operator/(const float rhs) const {
	return {x / rhs, y / rhs, z / rhs};
}

Vec3 Vec3::operator+(const float& rhs) const {
	return {x + rhs, y + rhs, z + rhs};
}

Vec3 Vec3::operator-(const float& rhs) const {
	return {x - rhs, y - rhs, z - rhs};
}

Vec3 Vec3::operator*(const Vec3& rhs) const {
	return {x * rhs.x, y * rhs.y, z * rhs.z};
}

Vec3 Vec3::operator/(const Vec3& rhs) const {
	return {x / rhs.x, y / rhs.y, z / rhs.z};
}

Vec3& Vec3::operator+=(const Vec3& rhs) {
	x += rhs.x;
	y += rhs.y;
	z += rhs.z;
	return *this;
}

Vec3& Vec3::operator-=(const Vec3& rhs) {
	x -= rhs.x;
	y -= rhs.y;
	z -= rhs.z;
	return *this;
}

Vec3& Vec3::operator*=(const float rhs) {
	x *= rhs;
	y *= rhs;
	z *= rhs;
	return *this;
}

Vec3& Vec3::operator/=(const float rhs) {
	x /= rhs;
	y /= rhs;
	z /= rhs;
	return *this;
}

Vec3 operator+(const float lhs, const Vec3& rhs) {
	return {rhs.x + lhs, rhs.y + lhs, rhs.z + lhs};
}

Vec3 operator-(const float lhs, const Vec3& rhs) {
	return {rhs.x - lhs, rhs.y - lhs, rhs.z - lhs};
}

Vec3 operator*(const float lhs, const Vec3& rhs) {
	return {rhs.x * lhs, rhs.y * lhs, rhs.z * lhs};
}

Vec3 operator/(const float lhs, const Vec3& rhs) {
	return {rhs.x / lhs, rhs.y / lhs, rhs.z / lhs};
}
