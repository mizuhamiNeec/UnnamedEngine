#include <cstdio>

#include <math/public/MathLib.h>

const Vec4 Vec4::zero = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
const Vec4 Vec4::one  = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

const Vec4 Vec4::red       = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
const Vec4 Vec4::green     = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
const Vec4 Vec4::blue      = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
const Vec4 Vec4::white     = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
const Vec4 Vec4::black     = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
const Vec4 Vec4::yellow    = Vec4(1.0f, 1.0f, 0.0f, 1.0f);
const Vec4 Vec4::cyan      = Vec4(0.0f, 1.0f, 1.0f, 1.0f);
const Vec4 Vec4::magenta   = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
const Vec4 Vec4::gray      = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
const Vec4 Vec4::lightGray = Vec4(.75f, .75f, .75f, 1.0f);
const Vec4 Vec4::darkGray  = Vec4(.25f, .25f, .25f, 1.0f);
const Vec4 Vec4::orange    = Vec4(1.0f, 0.5f, 0.0f, 1.0f);
const Vec4 Vec4::purple    = Vec4(0.5f, 0.0f, 0.5f, 1.0f);
const Vec4 Vec4::brown     = Vec4(0.5f, .25f, 0.0f, 1.0f);

Vec4::Vec4(const Vec3& vec3, const float& w):
	x(vec3.x), y(vec3.y), z(vec3.z), w(w) {
}

Vec4::Vec4(const __m128& v) {
	simd = v;
}

const char* Vec4::ToString() const {
	static thread_local char buf[64];
	std::snprintf(buf, sizeof(buf), "%.2f, %.2f, %.2f, %.2f", x, y, z, w);
	return buf;
}

Vec4 Vec4::operator+(const Vec4& vec4) const {
	Vec4 result;
	result.simd = _mm_add_ps(simd, vec4.simd);
	result.w    = 0.0f;
	return result;
}

Vec4 Vec4::operator-(const Vec4& vec4) const {
	Vec4 result;
	result.simd = _mm_sub_ps(simd, vec4.simd);
	result.w    = 0.0f;
	return result;
}

Vec4 Vec4::operator*(const Vec4& vec4) const {
	Vec4 result;
	result.simd = _mm_mul_ps(simd, vec4.simd);
	result.w    = 0.0f;
	return result;
}

Vec4 Vec4::operator/(const Vec4& vec4) const {
	Vec4 result;
	result.simd = _mm_div_ps(simd, vec4.simd);
	result.w    = 0.0f;
	return result;
}

Vec4 Vec4::operator*([[maybe_unused]] const Mat4& mat4) const {
	Vec4 result;
	// result.simd = _mm_mul_ps(simd, mat4.simd);
	// result.w    = 0.0f;
	return result;
}

Vec4 Vec4::operator*([[maybe_unused]] const float scalar) const {
	Vec4 result;
	// __m128 scalar = _mm_set1_ps(scalar);
	// result.simd   = _mm_mul_ps(simd, scalar);
	// result.w      = 0.0f;
	return result;
}

Vec4 Vec4::operator/(float scalar) const {
	Vec4 result;
	if (scalar != 0.0f) {
		__m128 scalarVec = _mm_set1_ps(scalar);
		result.simd      = _mm_div_ps(simd, scalarVec);
	} else {
		result.simd = simd; // ゼロ除算の回避
	}
	result.w = 0.0f;
	return result;
}
