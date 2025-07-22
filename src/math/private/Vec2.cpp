#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <math/public/Vec2.h>
//-----------------------------------------------------------------------------

const Vec2 Vec2::zero(0.0f, 0.0f);
const Vec2 Vec2::one(1.0f, 1.0f);
const Vec2 Vec2::up(0.0f, 1.0f);
const Vec2 Vec2::down(0.0f, -1.0f);
const Vec2 Vec2::right(1.0f, 0.0f);
const Vec2 Vec2::left(-1.0f, 0.0f);
const Vec2 Vec2::forward(0.0f, 1.0f);
const Vec2 Vec2::backward(0.0f, -1.0f);

const Vec2 Vec2::min(std::numeric_limits<float>::min());
const Vec2 Vec2::max(std::numeric_limits<float>::max());
const Vec2 Vec2::infinity(std::numeric_limits<float>::infinity());

float Vec2::Length() const {
	const float sqr = SqrLength();
	return (sqr > 0.0f) ? std::sqrt(sqr) : 0.0f;
}

float Vec2::SqrLength() const {
	__m128 mul = _mm_mul_ps(simd, simd);
	__m128 sum = _mm_hadd_ps(mul, mul);
	float  result;
	_mm_store_ss(&result, sum);
	return result;
}

float Vec2::Distance(const Vec2& other) const {
	Vec2 diff = *this - other;
	return diff.Length();
}

float Vec2::Dot(const Vec2& other) const {
	__m128 mul = _mm_mul_ps(simd, other.simd);
	__m128 sum = _mm_hadd_ps(mul, mul);
	float  result;
	_mm_store_ss(&result, sum);
	return result;
}

float Vec2::Cross(const Vec2& other) const {
	return x * other.y - y * other.x;
}

void Vec2::Normalize() {
	__m128 lengthSq  = _mm_dp_ps(simd, simd, 0x3F); // マスク0x3F: 結果を下位2要素に格納
	__m128 invLength = _mm_rsqrt_ps(lengthSq);
	simd             = _mm_mul_ps(simd, invLength);
	padding[0]       = 0.0f;
	padding[1]       = 0.0f;
}

Vec2 Vec2::Normalized() const {
	float len = Length();
	if (len > 0.0f) {
		return *this / len;
	}
	return zero;
}

Vec2 Vec2::Clamp(const Vec2 clampMin, const Vec2 clampMax) const {
	return {
		std::clamp(x, clampMin.x, clampMax.x),
		std::clamp(y, clampMin.y, clampMax.y)
	};
}

Vec2 Vec2::ClampLength(const float clampMin, const float clampMax) const {
	const float sqrLength = SqrLength();
	if (sqrLength > clampMax * clampMax) {
		const float scale = clampMax / std::sqrt(sqrLength);
		return {x * scale, y * scale};
	}
	if (sqrLength < clampMin * clampMin) {
		const float scale = clampMin / std::sqrt(sqrLength);
		return {x * scale, y * scale};
	}
	return {x, y};
}

Vec2 Vec2::Reflect(const Vec2& normal) const {
	return *this - 2.0f * Dot(normal) * normal;
}

Vec2 Vec2::RotateVector(const float angleZ) const {
	const float cosZ = std::cos(angleZ);
	const float sinZ = std::sin(angleZ);

	return {
		x * cosZ - y * sinZ,
		x * sinZ + y * cosZ
	};
}

const char* Vec2::ToString() const {
	static thread_local char buf[64];
	std::snprintf(buf, sizeof(buf), "%.2f, %.2f", x, y);
	return buf;
}

float& Vec2::operator[](const uint32_t index) {
	if (index < 2) {
		return (&x)[index];
	}
	throw std::out_of_range("Vec2 添字演算子に渡された値が範囲外です。");
}

const float& Vec2::operator[](const uint32_t index) const {
	if (index < 2) {
		return (&x)[index];
	}
	throw std::out_of_range("Vec2 添字演算子に渡された値が範囲外です。");
}

Vec2 Vec2::operator-() const {
	Vec2 result;
	result.simd       = _mm_sub_ps(_mm_setzero_ps(), simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator+(const Vec2& rhs) const {
	Vec2 result;
	result.simd       = _mm_add_ps(simd, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator-(const Vec2& rhs) const {
	Vec2 result;
	result.simd       = _mm_sub_ps(simd, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator*(const Vec2& rhs) const {
	Vec2 result;
	result.simd       = _mm_mul_ps(simd, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator/(const Vec2& rhs) const {
	Vec2 result;
	result.simd       = _mm_div_ps(simd, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator+(const float& rhs) const {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(rhs);
	result.simd       = _mm_add_ps(simd, scalar);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator-(const float& rhs) const {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(rhs);
	result.simd       = _mm_sub_ps(simd, scalar);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator*(const float& rhs) const {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(rhs);
	result.simd       = _mm_mul_ps(simd, scalar);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 Vec2::operator/(const float& rhs) const {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(rhs);
	result.simd       = _mm_div_ps(simd, scalar);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 operator+(const float lhs, const Vec2& rhs) {
	return rhs + lhs;
}

Vec2 operator-(const float lhs, const Vec2& rhs) {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(lhs);
	result.simd       = _mm_sub_ps(scalar, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2 operator*(const float lhs, const Vec2& rhs) {
	return rhs * lhs;
}

Vec2 operator/(const float lhs, const Vec2& rhs) {
	Vec2   result;
	__m128 scalar     = _mm_set1_ps(lhs);
	result.simd       = _mm_div_ps(scalar, rhs.simd);
	result.padding[0] = 0.0f;
	result.padding[1] = 0.0f;
	return result;
}

Vec2& Vec2::operator+=(const Vec2& rhs) {
	simd       = _mm_add_ps(simd, rhs.simd);
	padding[0] = 0.0f;
	padding[1] = 0.0f;
	return *this;
}

Vec2& Vec2::operator-=(const Vec2& rhs) {
	simd       = _mm_sub_ps(simd, rhs.simd);
	padding[0] = 0.0f;
	padding[1] = 0.0f;
	return *this;
}

Vec2& Vec2::operator*=(const Vec2& rhs) {
	simd       = _mm_mul_ps(simd, rhs.simd);
	padding[0] = 0.0f;
	padding[1] = 0.0f;
	return *this;
}

Vec2& Vec2::operator/=(const Vec2& rhs) {
	simd       = _mm_div_ps(simd, rhs.simd);
	padding[0] = 0.0f;
	padding[1] = 0.0f;
	return *this;
}

Vec2& Vec2::operator+=(const float& rhs) {
	__m128 scalar = _mm_set1_ps(rhs);
	simd          = _mm_add_ps(simd, scalar);
	padding[0]    = 0.0f;
	padding[1]    = 0.0f;
	return *this;
}

Vec2& Vec2::operator-=(const float& rhs) {
	__m128 scalar = _mm_set1_ps(rhs);
	simd          = _mm_sub_ps(simd, scalar);
	padding[0]    = 0.0f;
	padding[1]    = 0.0f;
	return *this;
}

Vec2& Vec2::operator*=(const float& rhs) {
	__m128 scalar = _mm_set1_ps(rhs);
	simd          = _mm_mul_ps(simd, scalar);
	padding[0]    = 0.0f;
	padding[1]    = 0.0f;
	return *this;
}

Vec2& Vec2::operator/=(const float& rhs) {
	__m128 scalar = _mm_set1_ps(rhs);
	simd          = _mm_div_ps(simd, scalar);
	padding[0]    = 0.0f;
	padding[1]    = 0.0f;
	return *this;
}
