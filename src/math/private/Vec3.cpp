#include <pch.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <math/public/MathLib.h>

//-----------------------------------------------------------------------------

const Vec3 Vec3::zero     = Vec3(0.0f, 0.0f, 0.0f);
const Vec3 Vec3::one      = Vec3(1.0f, 1.0f, 1.0f);
const Vec3 Vec3::up       = Vec3(0.0f, 0.0f, 1.0f);
const Vec3 Vec3::down     = Vec3(0.0f, 0.0f, -1.0f);
const Vec3 Vec3::right    = Vec3(1.0f, 0.0f, 0.0f);
const Vec3 Vec3::left     = Vec3(-1.0f, 0.0f, 0.0f);
const Vec3 Vec3::forward  = Vec3(0.0f, 1.0f, 0.0f);
const Vec3 Vec3::backward = Vec3(0.0f, -1.0f, 0.0f);

const Vec3 Vec3::min      = Vec3(std::numeric_limits<float>::min());
const Vec3 Vec3::max      = Vec3(std::numeric_limits<float>::max());
const Vec3 Vec3::infinity = Vec3(std::numeric_limits<float>::infinity());

Vec3::Vec3(const float x, const float y, const float z) :
	x(x),
	y(y),
	z(z),
	w(0.0f) {
}

Vec3::Vec3() : Vec3(0.0f, 0.0f, 0.0f) {
	w = 0.0f;
}

Vec3::Vec3(const float& value) : Vec3(value, value, value) {
}

Vec3::Vec3(const Vec2& vec2) : x(vec2.x),
                               y(vec2.y),
                               z(0.0f),
                               w(0.0f) {
}

Vec3::Vec3(Vec4& vec4) : x(vec4.x),
                         y(vec4.y),
                         z(vec4.z),
                         w(0.0f) {
}

Vec3::Vec3(const __m128& v) {
	simd = v;
	w    = 0.0f;
}

/**
 * @brief ベクトルの長さを返します。
 */
float Vec3::Length() const {
	const float sqr = SqrLength();
	return (sqr > 0.0f) ? std::sqrt(sqr) : 0.0f;
}

/**
 * @brief ベクトルの長さの二乗を返します。
 */
float Vec3::SqrLength() const {
	__m128 mul  = _mm_mul_ps(simd, simd);
	__m128 hadd = _mm_hadd_ps(mul, mul);
	hadd        = _mm_hadd_ps(hadd, hadd);
	float result;
	_mm_store_ss(&result, hadd);
	return result;
}

/**
 * @brief ベクトルの距離を返します。
 * @param other 比較対象のベクトル
 */
float Vec3::Distance(const Vec3& other) const {
	Vec3 diff = *this - other;
	return diff.Length();
}

/**
 * @brief Dot積を計算して返します。
 * @param other 比較対象のベクトル
 */
float Vec3::Dot(const Vec3& other) const {
	__m128 dp   = _mm_mul_ps(simd, other.simd);
	__m128 shuf = _mm_shuffle_ps(dp, dp, _MM_SHUFFLE(2, 1, 0, 3));
	__m128 sums = _mm_add_ps(dp, shuf);
	shuf        = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(1, 0, 3, 2));
	sums        = _mm_add_ss(sums, shuf);
	float result;
	_mm_store_ss(&result, sums);
	return result;
}

/**
 * @brief Cross積を計算して返します。
 * @param other 比較対象のベクトル
 */
Vec3 Vec3::Cross(const Vec3& other) const {
	__m128 tmp0 = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3, 0, 2, 1));
	// (y, z, x, w)
	__m128 tmp1 = _mm_shuffle_ps(other.simd, other.simd,
	                             _MM_SHUFFLE(3, 1, 0, 2)); // (z, x, y, w)
	__m128 tmp2 = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3, 1, 0, 2));
	// (z, x, y, w)
	__m128 tmp3 = _mm_shuffle_ps(other.simd, other.simd,
	                             _MM_SHUFFLE(3, 0, 2, 1)); // (y, z, x, w)
	__m128 result = _mm_sub_ps(_mm_mul_ps(tmp0, tmp1), _mm_mul_ps(tmp2, tmp3));
	return Vec3(result);
}

/**
 * @brief ベクトルを正規化します。
 * 正規化されたベクトルは長さが1になります。
 */
void Vec3::Normalize() {
	__m128 lengthSq  = _mm_dp_ps(simd, simd, 0x7F);
	__m128 invLength = _mm_rsqrt_ps(lengthSq);
	simd             = _mm_mul_ps(simd, invLength);
	w                = 0.0f;
}

/**
 * @brief 正規化されたベクトルを返します。
 * 正規化されたベクトルは長さが1になります。
 * 長さが0の場合はゼロベクトルを返します。
 */
Vec3 Vec3::Normalized() const {
	float len = Length();
	if (len > 0.0f) {
		return *this / len;
	}
	return zero;
}

/**
 * @brief ベクトルをクランプした値を返します。
 * @param clampMin クランプの最小値
 * @param clampMax クランプの最大値
 */
Vec3 Vec3::Clamp(const Vec3 clampMin, const Vec3 clampMax) const {
	return {
		std::clamp(x, clampMin.x, clampMax.x),
		std::clamp(y, clampMin.y, clampMax.y),
		std::clamp(z, clampMin.z, clampMax.z)
	};
}

/**
 * @brief ベクトルの長さをクランプした値を返します。
 * @param clampMin クランプの最小値
 * @param clampMax クランプの最大値
 */
Vec3 Vec3::ClampLength(const float clampMin, const float clampMax) const {
	const float sqrLength = SqrLength();
	if (sqrLength > clampMax * clampMax) {
		const float scale = clampMax / std::sqrt(sqrLength);
		return {x * scale, y * scale, z * scale};
	}
	if (sqrLength < clampMin * clampMin) {
		const float scale = clampMin / std::sqrt(sqrLength);
		return {x * scale, y * scale, z * scale};
	}
	return {x, y, z};
}

/**
 * @brief ベクトルを反射させた値を返します。
 * @param normal 反射の基準となる法線ベクトル
 */
Vec3 Vec3::Reflect(const Vec3& normal) const {
	return *this - 2.0f * Dot(normal) * normal;
}

/**
 * @brief ベクトルを文字列に変換して返します。
 */
const char* Vec3::ToString() const {
	static thread_local char buf[64];
	std::snprintf(buf, sizeof(buf), "%.2f, %.2f, %.2f", x, y, z);
	return buf;
}

Vec3 Vec3::Abs() {
	x = std::fabs(x);
	y = std::fabs(y);
	z = std::fabs(z);
	return *this;
}

/// @brief ベクトルがゼロベクトルかどうかを判定します。
/// @param threshold ゼロとみなす閾値
/// @return ゼロベクトルであればtrue、そうでなければfalse
bool Vec3::IsZero(const float threshold) const {
	return std::fabs(x) < threshold &&
		std::fabs(y) < threshold &&
		std::fabs(z) < threshold;
}

/// @brief ベクトルが他のベクトルと平行かどうかを判定します。
/// @param dir 判定対象のベクトル
/// @return 平行であればtrue、そうでなければfalse
bool Vec3::IsParallel(const Vec3 dir) const {
	return std::fabs(Dot(dir.Normalized())) > 0.999f;
}

/**
 * @brief ベクトルの要素を添字演算子で取得します。
 * @param index 添字
 * @return 要素の参照
 */
float& Vec3::operator[](const uint32_t index) {
	if (index < 3) {
		return (&x)[index];
	}
	throw std::out_of_range("Vec3 添字演算子に渡された値が範囲外です。");
}

/**
 * @brief ベクトルの要素を添字演算子で取得します。
 * @param index 添字
 * @return 要素の参照
 */
const float& Vec3::operator[](const uint32_t index) const {
	if (index < 3) {
		return (&x)[index];
	}
	throw std::out_of_range("Vec3 添字演算子に渡された値が範囲外です。");
}

Vec3 Vec3::operator-() const {
	Vec3 result;
	result.simd = _mm_sub_ps(_mm_setzero_ps(), simd);
	result.w    = 0.0f;
	return result;
}

Vec3 Vec3::operator+(const Vec3& rhs) const {
	Vec3 result;
	result.simd = _mm_add_ps(simd, rhs.simd);
	result.w    = 0.0f;
	return result;
}

Vec3 Vec3::operator-(const Vec3& rhs) const {
	Vec3 result;
	result.simd = _mm_sub_ps(simd, rhs.simd);
	result.w    = 0.0f;
	return result;
}

Vec3 Vec3::operator*(const Vec3& rhs) const {
	Vec3 result;
	result.simd = _mm_mul_ps(simd, rhs.simd);
	result.w    = 0.0f;
	return result;
}

Vec3 Vec3::operator/(const Vec3& rhs) const {
	Vec3 result;
	result.simd = _mm_div_ps(simd, rhs.simd);
	result.w    = 0.0f;
	return result;
}

Vec3 Vec3::operator+(const float& rhs) const {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(rhs);
	result.simd   = _mm_add_ps(simd, scalar);
	result.w      = 0.0f;
	return result;
}

Vec3 Vec3::operator-(const float& rhs) const {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(rhs);
	result.simd   = _mm_sub_ps(simd, scalar);
	result.w      = 0.0f;
	return result;
}

Vec3 Vec3::operator*(const float& rhs) const {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(rhs);
	result.simd   = _mm_mul_ps(simd, scalar);
	result.w      = 0.0f;
	return result;
}

Vec3 Vec3::operator/(const float& rhs) const {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(rhs);
	result.simd   = _mm_div_ps(simd, scalar);
	result.w      = 0.0f;
	return result;
}

Vec3 operator+(const float lhs, const Vec3& rhs) {
	return rhs + lhs;
}

Vec3 operator-(const float lhs, const Vec3& rhs) {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(lhs);
	result.simd   = _mm_sub_ps(scalar, rhs.simd);
	result.w      = 0.0f;
	return result;
}

Vec3 operator*(const float lhs, const Vec3& rhs) {
	return rhs * lhs;
}

Vec3 operator/(const float lhs, const Vec3& rhs) {
	Vec3   result;
	__m128 scalar = _mm_set1_ps(lhs);
	result.simd   = _mm_div_ps(scalar, rhs.simd);
	result.w      = 0.0f;
	return result;
}

Vec3& Vec3::operator+=(const Vec3& rhs) {
	simd = _mm_add_ps(simd, rhs.simd);
	w    = 0.0f;
	return *this;
}

Vec3& Vec3::operator-=(const Vec3& rhs) {
	simd = _mm_sub_ps(simd, rhs.simd);
	w    = 0.0f;
	return *this;
}

Vec3& Vec3::operator*=(const Vec3& rhs) {
	simd = _mm_mul_ps(simd, rhs.simd);
	w    = 0.0f;
	return *this;
}

Vec3& Vec3::operator/=(const Vec3& rhs) {
	simd = _mm_div_ps(simd, rhs.simd);
	w    = 0.0f;
	return *this;
}

Vec3& Vec3::operator+=(const float& rhs) {
	return *this = *this + rhs;
}

Vec3& Vec3::operator-=(const float& rhs) {
	return *this = *this - rhs;
}

Vec3& Vec3::operator*=(const float& rhs) {
	return *this = *this * rhs;
}

Vec3& Vec3::operator/=(const float& rhs) {
	return *this = *this / rhs;
}

bool Vec3::operator!=(const Vec3& rhs) const {
	return x != rhs.x || y != rhs.y || z != rhs.z;
}
