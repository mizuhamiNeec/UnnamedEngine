#include <math/public/MathLib.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

const Vec3 Vec3::zero(0.0f, 0.0f, 0.0f);
const Vec3 Vec3::one(1.0f, 1.0f, 1.0f);
const Vec3 Vec3::right(1.0f, 0.0f, 0.0f);
const Vec3 Vec3::left(-1.0f, 0.0f, 0.0f);
const Vec3 Vec3::up(0.0f, 1.0f, 0.0f);
const Vec3 Vec3::down(0.0f, -1.0f, 0.0f);
const Vec3 Vec3::forward(0.0f, 0.0f, 1.0f);
const Vec3 Vec3::backward(0.0f, 0.0f, -1.0f);
const Vec3 Vec3::max(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
const Vec3 Vec3::min(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

float Vec3::Length() const {
	if (const float sqrLength = SqrLength(); sqrLength > 0.0f) {
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
	return { y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x };
}

bool Vec3::IsZero(const float tolerance) const {
	return std::fabs(x) < tolerance && std::fabs(y) < tolerance;
}

bool Vec3::IsParallel(const Vec3& other) const {
	const Vec3 normalizedOther = other.Normalized();
	const float dot = Normalized().Dot(normalizedOther);
	return abs(abs(dot) - 1.0f) < 1e-6;
}

void Vec3::Normalize() {
	if (const float len = Length(); len > 0) {
		x /= len;
		y /= len;
		z /= len;
	}
}

Vec3 Vec3::Normalized() const {
	if (const float len = Length(); len > 0) {
		return { x / len, y / len, z / len };
	}
	return zero;
}

Vec3 Vec3::Clamp(const Vec3 minVec, const Vec3 maxVec) const {
	return {
		std::clamp(x, minVec.x, maxVec.x),
		std::clamp(y, minVec.y, maxVec.y),
		std::clamp(z, minVec.z, maxVec.z)
	};
}

Vec3 Vec3::ClampLength(const float minVec, const float maxVec) {
	const float sqrLength = SqrLength();
	if (sqrLength > maxVec * maxVec) {
		const float scale = maxVec / std::sqrt(sqrLength);
		return { x * scale, y * scale, z * scale };
	}
	if (sqrLength < minVec * minVec) {
		const float scale = minVec / std::sqrt(sqrLength);
		return { x * scale, y * scale, z * scale };
	}
	return { x, y, z };
}

Vec3 Vec3::Reflect(const Vec3& normal) const {
	return *this - 2 * this->Dot(normal) * normal;
}

Vec3 Vec3::Abs() {
	x = std::abs(x);
	y = std::abs(y);
	z = std::abs(z);
	return *this;
}

Vec3 Vec3::TransformDirection(const Quaternion& rotation) const {
	Mat4 rotationMat = Mat4::RotateQuaternion(rotation);
	return Mat4::Transform(*this, rotationMat);
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

Vec3 Vec3::operator-() const {
	return { -x, -y, -z };
}

Vec3 Vec3::operator+(const Vec3& rhs) const {
	return { x + rhs.x, y + rhs.y, z + rhs.z };
}

Vec3 Vec3::operator-(const Vec3& rhs) const {
	return { x - rhs.x, y - rhs.y, z - rhs.z };
}

Vec3 Vec3::operator*(const float rhs) const {
	return { x * rhs, y * rhs, z * rhs };
}

Vec3 Vec3::operator/(const float rhs) const {
	return { x / rhs, y / rhs, z / rhs };
}

Vec3 Vec3::operator+(const float& rhs) const {
	return { x + rhs, y + rhs, z + rhs };
}

Vec3 Vec3::operator-(const float& rhs) const {
	return { x - rhs, y - rhs, z - rhs };
}

Vec3 Vec3::operator*(const Vec3& rhs) const {
	return { x * rhs.x, y * rhs.y, z * rhs.z };
}

Vec3 Vec3::operator/(const Vec3& rhs) const {
	return { x / rhs.x, y / rhs.y, z / rhs.z };
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
	return { rhs.x + lhs, rhs.y + lhs, rhs.z + lhs };
}

Vec3 operator-(const float lhs, const Vec3& rhs) {
	return { rhs.x - lhs, rhs.y - lhs, rhs.z - lhs };
}

Vec3 operator*(const float lhs, const Vec3& rhs) {
	return { rhs.x * lhs, rhs.y * lhs, rhs.z * lhs };
}

Vec3 operator/(const float lhs, const Vec3& rhs) {
	return { rhs.x / lhs, rhs.y / lhs, rhs.z / lhs };
}

std::string Vec3::ToString() const {
	return std::format("({:.2f}, {:.2f}, {:.2f})", x, y, z);
}

bool Vec3::operator!=(const Vec3& rhs) const {
	return x != rhs.x || y != rhs.y || z != rhs.z;
}

bool Vec3::operator==(const Vec3& vec3) const {
	return x == vec3.x && y == vec3.y && z == vec3.z;
}

Vec3 Vec3::Min(const Vec3 lhs, const Vec3 rhs) {
	return {
		std::min(lhs.x, rhs.x),
		std::min(lhs.y, rhs.y),
		std::min(lhs.z, rhs.z)
	};
}

Vec3 Vec3::Max(const Vec3 lhs, const Vec3 rhs) {
	return {
		std::max(lhs.x, rhs.x),
		std::max(lhs.y, rhs.y),
		std::max(lhs.z, rhs.z)
	};
}