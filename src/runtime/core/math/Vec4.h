#pragma once

#include <initializer_list>

struct Mat4;
#include <runtime/core/math/Vec3.h>

/**
 * @brief 4次元ベクトル構造体
 */
struct Vec4 final {
	/**
	 * @brief コンストラクタ
	 * @param x X成分（デフォルト: 0.0f）
	 * @param y Y成分（デフォルト: 0.0f）
	 * @param z Z成分（デフォルト: 0.0f）
	 * @param w W成分（デフォルト: 0.0f）
	 */
	constexpr Vec4(const float x = 0.0f, const float y = 0.0f,
	               const float z = 0.0f, const float w = 0.0f) : x(x),
		y(y),
		z(z),
		w(w) {
	}

	/**
	 * @brief Vec3とW成分から初期化するコンストラクタ
	 * @param vec3 3次元ベクトル
	 * @param w W成分
	 */
	constexpr Vec4(const Vec3 vec3, const float w) : x(vec3.x),
		y(vec3.y),
		z(vec3.z),
		w(w) {
	}

	/**
	 * @brief 初期化リストから生成するコンストラクタ
	 * @param list 初期化リスト
	 */
	constexpr Vec4(const std::initializer_list<float> list) {
		auto it = list.begin();
		x       = (it != list.end()) ? *it++ : 0.0f;
		y       = (it != list.end()) ? *it++ : 0.0f;
		z       = (it != list.end()) ? *it++ : 0.0f;
		w       = (it != list.end()) ? *it++ : 0.0f;
	}

	float       x, y, z, w;
	static Vec4 one;  ///< 単位ベクトル (1, 1, 1, 1)
	static Vec4 zero; ///< ゼロベクトル (0, 0, 0, 0)

	static Vec4 red;       ///< 赤色 (1, 0, 0, 1)
	static Vec4 green;     ///< 緑色 (0, 1, 0, 1)
	static Vec4 blue;      ///< 青色 (0, 0, 1, 1)
	static Vec4 white;     ///< 白色 (1, 1, 1, 1)
	static Vec4 black;     ///< 黒色 (0, 0, 0, 1)
	static Vec4 yellow;    ///< 黄色 (1, 1, 0, 1)
	static Vec4 cyan;      ///< シアン色 (0, 1, 1, 1)
	static Vec4 magenta;   ///< マゼンタ色 (1, 0, 1, 1)
	static Vec4 gray;      ///< 灰色 (0.5, 0.5, 0.5, 1)
	static Vec4 lightGray; ///< 明るい灰色 (0.75, 0.75, 0.75, 1)
	static Vec4 darkGray;  ///< 暗い灰色 (0.25, 0.25, 0.25, 1)
	static Vec4 orange;    ///< オレンジ色 (1, 0.5, 0, 1)
	static Vec4 purple;    ///< 紫色 (0.5, 0, 0.5, 1)
	static Vec4 brown;     ///< 茶色 (0.6, 0.3, 0, 1)

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
