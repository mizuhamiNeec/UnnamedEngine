#pragma once

//-----------------------------------------------------------------------------
// x86_64 専用 TODO: やる気と時間ができたら他のアーキテクチャに対応する
//-----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4201) // warning C4201: nonstandard extension used: nameless struct/union


struct Vec2;
struct Vec4;

struct alignas(16) Vec3 {
	union {
		// w はパディング用
		struct {
			float x, y, z, w;
		};

		__m128 simd;
	};

	static const Vec3 zero;
	static const Vec3 one;
	static const Vec3 up;
	static const Vec3 down;
	static const Vec3 right;
	static const Vec3 left;
	static const Vec3 forward;
	static const Vec3 backward;
	static const Vec3 min;
	static const Vec3 max;
	static const Vec3 infinity;

	Vec3(float x, float y, float z);
	Vec3();
	explicit Vec3(const float& value);
	explicit Vec3(const Vec2& vec2);
	explicit Vec3(Vec4& vec4);
	explicit Vec3(const __m128& v);

	//---------------------------------------------------------------------
	// Functions
	//---------------------------------------------------------------------
	[[nodiscard]] float Length() const;
	[[nodiscard]] float SqrLength() const;
	[[nodiscard]] float Distance(const Vec3& other) const;
	[[nodiscard]] float Dot(const Vec3& other) const;
	[[nodiscard]] Vec3  Cross(const Vec3& other) const;

	void               Normalize();
	[[nodiscard]] Vec3 Normalized() const;

	[[nodiscard]] Vec3 Clamp(Vec3 clampMin, Vec3 clampMax) const;
	[[nodiscard]] Vec3 ClampLength(float clampMin, float clampMax) const;

	[[nodiscard]] Vec3 Reflect(const Vec3& normal) const;

	[[nodiscard]] const char* ToString() const;

	Vec3 Abs();

	bool IsZero(float threshold = 0.0001f) const;
	bool IsParallel(Vec3 dir) const;

	//---------------------------------------------------------------------
	// Operators
	//---------------------------------------------------------------------
	float&       operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec3 operator-() const;

	Vec3 operator+(const Vec3& rhs) const;
	Vec3 operator-(const Vec3& rhs) const;
	Vec3 operator*(const Vec3& rhs) const;
	Vec3 operator/(const Vec3& rhs) const;

	Vec3 operator+(const float& rhs) const;
	Vec3 operator-(const float& rhs) const;
	Vec3 operator*(const float& rhs) const;
	Vec3 operator/(const float& rhs) const;

	friend Vec3 operator+(float lhs, const Vec3& rhs);
	friend Vec3 operator-(float lhs, const Vec3& rhs);
	friend Vec3 operator*(float lhs, const Vec3& rhs);
	friend Vec3 operator/(float lhs, const Vec3& rhs);

	Vec3& operator+=(const Vec3& rhs);
	Vec3& operator-=(const Vec3& rhs);
	Vec3& operator*=(const Vec3& rhs);
	Vec3& operator/=(const Vec3& rhs);

	Vec3& operator+=(const float& rhs);
	Vec3& operator-=(const float& rhs);
	Vec3& operator*=(const float& rhs);
	Vec3& operator/=(const float& rhs);

	bool operator!=(const Vec3& rhs) const;
};
