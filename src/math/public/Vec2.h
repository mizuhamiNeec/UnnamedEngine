#pragma once

//-----------------------------------------------------------------------------
// x86_64 専用 TODO: やる気と時間ができたら他のアーキテクチャに対応する
//-----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4201) // warning C4201: nonstandard extension used: nameless struct/union
#include <cstdint>
#include <xmmintrin.h>

struct alignas(16) Vec2 final {
	union {
		struct {
			float x, y;
			float padding[2];
		};

		__m128 simd;
	};

	static const Vec2 zero;
	static const Vec2 one;
	static const Vec2 up;
	static const Vec2 down;
	static const Vec2 right;
	static const Vec2 left;

	static const Vec2 forward;
	static const Vec2 backward;
	static const Vec2 min;
	static const Vec2 max;
	static const Vec2 infinity;

	Vec2() : x(0.0f),
		y(0.0f) {
	}

	Vec2(const float x, const float y) : x(x),
		y(y) {
	}

	Vec2(const float& value) : x(value),
		y(value) {
	}

	explicit Vec2(const __m128 v) { simd = v; }

	//---------------------------------------------------------------------
	// Functions
	//---------------------------------------------------------------------
	[[nodiscard]] float Length() const;
	[[nodiscard]] float SqrLength() const;
	[[nodiscard]] float Distance(const Vec2& other) const;
	[[nodiscard]] float Dot(const Vec2& other) const;
	[[nodiscard]] float Cross(const Vec2& other) const;

	void               Normalize();
	[[nodiscard]] Vec2 Normalized() const;

	[[nodiscard]] Vec2 Clamp(Vec2 clampMin, Vec2 clampMax) const;
	[[nodiscard]] Vec2 ClampLength(float clampMin, float clampMax) const;

	[[nodiscard]] Vec2 Reflect(const Vec2& normal) const;

	[[nodiscard]] Vec2 RotateVector(float angleZ) const;

	[[nodiscard]] const char* ToString() const;

	//---------------------------------------------------------------------
	// Operators
	//---------------------------------------------------------------------
	float&       operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec2 operator-() const;

	Vec2 operator+(const Vec2& rhs) const;
	Vec2 operator-(const Vec2& rhs) const;
	Vec2 operator*(const Vec2& rhs) const;
	Vec2 operator/(const Vec2& rhs) const;

	Vec2 operator+(const float& rhs) const;
	Vec2 operator-(const float& rhs) const;
	Vec2 operator*(const float& rhs) const;
	Vec2 operator/(const float& rhs) const;

	friend Vec2 operator+(float lhs, const Vec2& rhs);
	friend Vec2 operator-(float lhs, const Vec2& rhs);
	friend Vec2 operator*(float lhs, const Vec2& rhs);
	friend Vec2 operator/(float lhs, const Vec2& rhs);

	Vec2& operator+=(const Vec2& rhs);
	Vec2& operator-=(const Vec2& rhs);
	Vec2& operator*=(const Vec2& rhs);
	Vec2& operator/=(const Vec2& rhs);

	Vec2& operator+=(const float& rhs);
	Vec2& operator-=(const float& rhs);
	Vec2& operator*=(const float& rhs);
	Vec2& operator/=(const float& rhs);
};
