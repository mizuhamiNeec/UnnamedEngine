#pragma once

#include <xmmintrin.h>
#include <initializer_list>

#pragma warning(push)
#pragma warning(disable : 4201) // warning C4201: nonstandard extension used: nameless struct/union



struct Vec3;
struct Mat4;

struct alignas(16) Vec4 {
	union {
		struct {
			float x, y, z, w;
		};

		__m128 simd;
	};

	static const Vec4 zero;
	static const Vec4 one;

	// RGB BW
	static const Vec4 red;
	static const Vec4 green;
	static const Vec4 blue;
	static const Vec4 white;
	static const Vec4 black;

	static const Vec4 yellow;
	static const Vec4 cyan;
	static const Vec4 magenta;
	static const Vec4 gray;
	static const Vec4 lightGray;
	static const Vec4 darkGray;
	static const Vec4 orange;
	static const Vec4 purple;
	static const Vec4 brown;


	constexpr Vec4() : x(0), y(0), z(0), w(0) {
	}

	constexpr Vec4(const float& x, const float& y, const float& z,
	               const float& w)
		: x(x), y(y), z(z), w(w) {
	}

	explicit Vec4(const Vec3& vec3, const float& w = 1.0f);

	constexpr Vec4(
		const std::initializer_list<float>& list
	)
		:
		x(0), y(0), z(0), w(0) {
		auto it = list.begin();
		if (list.size() > 0) x = *it++;
		if (list.size() > 1) y = *it++;
		if (list.size() > 2) z = *it++;
		if (list.size() > 3) w = *it++;
	}

	explicit Vec4(
		const __m128& v
	);

	[[nodiscard]] const char* ToString() const;

	//-------------------------------------------------------------------------
	// Operators
	//-------------------------------------------------------------------------
	Vec4 operator+(const Vec4& vec4) const;
	Vec4 operator-(const Vec4& vec4) const;
	Vec4 operator*(const Vec4& vec4) const;
	Vec4 operator/(const Vec4& vec4) const;

	Vec4 operator*(const Mat4& mat4) const;
	Vec4 operator*(float scalar) const;
	Vec4 operator/(float scalar) const;
};
