#pragma once

#include <initializer_list>

struct Mat4;
#include <math/public/Vec3.h>

struct Vec4 final {
	constexpr Vec4(const float x = 0.0f, const float y = 0.0f,
	               const float z = 0.0f, const float w = 0.0f) : x(x),
		y(y),
		z(z),
		w(w) {
	}

	constexpr Vec4(const Vec3 vec3, const float w) : x(vec3.x),
		y(vec3.y),
		z(vec3.z),
		w(w) {
	}

	constexpr Vec4(const std::initializer_list<float> list) {
		auto it = list.begin();
		x       = (it != list.end()) ? *it++ : 0.0f;
		y       = (it != list.end()) ? *it++ : 0.0f;
		z       = (it != list.end()) ? *it++ : 0.0f;
		w       = (it != list.end()) ? *it++ : 0.0f;
	}

	float       x, y, z, w;
	static Vec4 one;
	static Vec4 zero;

	static Vec4 red;
	static Vec4 green;
	static Vec4 blue;
	static Vec4 white;
	static Vec4 black;
	static Vec4 yellow;
	static Vec4 cyan;
	static Vec4 magenta;
	static Vec4 gray;
	static Vec4 lightGray;
	static Vec4 darkGray;
	static Vec4 orange;
	static Vec4 purple;
	static Vec4 brown;

	constexpr float&       operator[](int index);
	constexpr const float& operator[](int index) const;

	Vec4 operator*(const Mat4& mat4) const;
	Vec4 operator*(float rhs) const;
	Vec4 operator+(const Vec4& vec4) const;
	Vec4 operator/(float rhs) const;
};

#ifdef _DEBUG
#include <imgui.h>
ImVec4 ToImVec4(const Vec4& vec);
#endif
