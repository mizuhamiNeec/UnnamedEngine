#include "Vec3.h"

#include <stdexcept>

Vec3::Vec3() = default;

Vec3::Vec3(const float& x, const float& y, const float& z) : x(x), y(y), z(z) {
}

Vec3 Vec3::Zero() {
	return {0.0f, 0.0f, 0.0f};
}

Vec3 Vec3::Right() {
	return {1.0f, 0.0f, 0.0f};
}

Vec3 Vec3::Forward() {
	return {0.0f, 1.0f, 0.0f};
}

Vec3 Vec3::Up() {
	return {0.0f, 0.0f, 1.0f};
}

float Vec3::Length() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		return sqrtf(sqrLength);
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
	return Vec3(distX, distY, distZ).Length();
}

float Vec3::DotProduct(const Vec3& other) const {
	return x * other.x + y * other.y + z * other.z;
}

Vec3 Vec3::CrossProduct(const Vec3& other) const {
	return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
}

Vec3 Vec3::Normalize() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		const float invLength = 1.0f / sqrtf(sqrLength);
		return {x * invLength, y * invLength, z * invLength};
	}
	return Zero();
}

float& Vec3::operator[](const int index) {
	if (index == 0) {
		return x;
	}
	if (index == 1) {
		return y;
	}
	if (index == 2) {
		return z;
	}
	throw std::out_of_range("Vec3型の添字演算子に無効なインデックスが渡されました");
}

const float& Vec3::operator[](const int index) const {
	if (index == 0) {
		return x;
	}
	if (index == 1) {
		return y;
	}
	if (index == 2) {
		return z;
	}
	throw std::out_of_range("Vec3型の添字演算子に無効なインデックスが渡されました");
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

Vec3& Vec3::operator*=(const float& rhs) {
	x *= rhs;
	y *= rhs;
	z *= rhs;
	return *this;
}

Vec3& Vec3::operator/=(const float& rhs) {
	x /= rhs;
	y /= rhs;
	z /= rhs;
	return *this;
}
