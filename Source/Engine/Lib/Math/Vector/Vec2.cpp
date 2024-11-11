#include "Vec2.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

const Vec2 Vec2::zero(0.0f, 0.0f);
const Vec2 Vec2::right(1.0f, 0.0f);
const Vec2 Vec2::left(-1.0f, 0.0f);
const Vec2 Vec2::up(0.0f, 1.0f);
const Vec2 Vec2::down(0.0f, -1.0f);

float& Vec2::operator[](const uint32_t index) {
	switch (index) {
	case 0: return x;
	case 1: return y;
	default: throw std::out_of_range("Vec2 添字演算子");
	}
}

const float& Vec2::operator[](const uint32_t index) const {
	switch (index) {
	case 0: return x;
	case 1: return y;
	default: throw std::out_of_range("Vec2 添字演算子");
	}
}

Vec2 Vec2::operator+(const Vec2& rhs) const {
	return { x + rhs.x , y + rhs.y };
}

Vec2 Vec2::operator-(const Vec2& rhs) const {
	return { x - rhs.x, y - rhs.y };
}

Vec2 Vec2::operator*(const float rhs) const {
	return { x * rhs,y * rhs };
}

Vec2 Vec2::operator/(const float rhs) const {
	if (rhs == 0.0f) {
		throw std::invalid_argument("ゼロ除算!!");
	}
	return { x / rhs, y / rhs };
}

Vec2 Vec2::operator+(const float& rhs) const {
	return { x + rhs, y + rhs };
}

Vec2 Vec2::operator-(const float& rhs) const {
	return { x - rhs, y - rhs };
}

Vec2 Vec2::operator*(const Vec2& rhs) const {
	return { x * rhs.x, y * rhs.y };
}

Vec2 Vec2::operator/(const Vec2& rhs) const {
	if (rhs.x == 0.0f || rhs.y == 0.0f) {
		throw std::invalid_argument("ゼロ除算!!");
	}
	return { x / rhs.x, y / rhs.y };
}

Vec2& Vec2::operator+=(const Vec2& rhs) {
	x += rhs.x;
	y += rhs.y;
	return *this;
}

Vec2& Vec2::operator-=(const Vec2& rhs) {
	x -= rhs.x;
	y -= rhs.y;
	return *this;
}

Vec2& Vec2::operator*=(const float rhs) {
	x *= rhs;
	y *= rhs;
	return *this;
}

Vec2& Vec2::operator/=(const float rhs) {
	x /= rhs;
	y /= rhs;
	return *this;
}

Vec2 operator+(const float lhs, const Vec2& rhs) {
	return { lhs + rhs.x, lhs + rhs.y };
}

Vec2 operator-(const float lhs, const Vec2& rhs) {
	return { lhs - rhs.x, lhs - rhs.y };
}

Vec2 operator*(const float lhs, const Vec2& rhs) {
	return { lhs * rhs.x, lhs * rhs.y };
}

Vec2 operator/(const float lhs, const Vec2& rhs) {
	return { lhs / rhs.x, lhs / rhs.y };
}

Vec2::Vec2(const float x, const float y) : x(x), y(y) {}

float Vec2::Length() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		return std::sqrt(sqrLength);
	}
	return 0.0f;
}

float Vec2::SqrLength() const {
	return x * x + y * y;
}

float Vec2::Distance(const Vec2& other) const {
	const float distX = other.x - x;
	const float distY = other.y - y;
	return std::sqrt(distX * distX + distY * distY);
}

float Vec2::Dot(const Vec2& other) const {
	return x * other.x + y * other.y;
}

float Vec2::Cross(const Vec2& other) const {
	return x * other.y - y * other.x;
}

bool Vec2::IsZero(const float tolerance) const {
	return std::fabs(x) < tolerance && std::fabs(y) < tolerance;
}

void Vec2::Normalize() {
	const float len = Length();
	if (len > 0) {
		x /= len;
		y /= len;
	} else {
		throw std::invalid_argument("ゼロ除算!!");
	}
}

Vec2 Vec2::Normalized() const {
	float len = Length();
	if (len > 0) {
		return { x / len, y / len };
	}
	throw std::invalid_argument("ゼロ除算!!");
}

Vec2 Vec2::Clamp(const Vec2 min, const Vec2 max) const {
	return {
		std::clamp(x,min.x,max.x),
		std::clamp(y, min.y,max.y)
	};
}

Vec2 Vec2::ClampLength(const float min, const float max) {
	const float sqrLength = SqrLength();
	if (sqrLength > max * max) {
		const float scale = max / std::sqrt(sqrLength);
		return { x * scale, y * scale };
	}
	if (sqrLength < min * min) {
		if (sqrLength == 0.0f) {
			return { 0.0f, 0.0f };
		}
		const float scale = min / std::sqrt(sqrLength);
		return { x * scale, y * scale };
	}
	return { x, y };
}

Vec2 Vec2::Lerp(const Vec2& target, float t) const {
	return *this * (1 - t) + target * t;
}

Vec2 Vec2::Reflect(const Vec2& normal) const {
	return *this - 2 * this->Dot(normal) * normal;
}

Vec2 Vec2::RotateVector(const float angleZ) const {
	const float cos = std::cos(angleZ);
	const float sin = std::sin(angleZ);
	return { x * cos - y * sin, x * sin + y * cos };
}