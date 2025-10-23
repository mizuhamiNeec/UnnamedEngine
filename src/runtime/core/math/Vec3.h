#pragma once
#include <cstdint>
#include <string>

struct Quaternion;
#include <runtime/core/math/Vec2.h>

/**
 * @brief 3次元ベクトル構造体
 */
struct Vec3 final {
	float x, y, z;

	static const Vec3 zero;     ///< ゼロベクトル (0, 0, 0)
	static const Vec3 one;      ///< 単位ベクトル (1, 1, 1)
	static const Vec3 right;    ///< 右方向 (1, 0, 0)
	static const Vec3 left;     ///< 左方向 (-1, 0, 0)
	static const Vec3 up;       ///< 上方向 (0, 1, 0)
	static const Vec3 down;     ///< 下方向 (0, -1, 0)
	static const Vec3 forward;  ///< 前方向 (0, 0, 1)
	static const Vec3 backward; ///< 後方向 (0, 0, -1)
	static const Vec3 max;      ///< 最大値
	static const Vec3 min;      ///< 最小値

	/**
	 * @brief デフォルトコンストラクタ
	 */
	constexpr Vec3() : Vec3(0.0f, 0.0f, 0.0f) {
	}

	/**
	 * @brief コンストラクタ
	 * @param x X成分
	 * @param y Y成分
	 * @param z Z成分
	 */
	constexpr
	Vec3(const float x, const float y, const float z) : x(x), y(y), z(z) {
	}

	/**
	 * @brief スカラー値から初期化するコンストラクタ
	 * @param scalar 全成分に設定する値
	 */
	explicit constexpr Vec3(const float scalar) : x(scalar), y(scalar),
	                                              z(scalar) {
	}

	/**
	 * @brief Vec2から変換するコンストラクタ
	 * @param vec2 2次元ベクトル（z成分は0に設定）
	 */
	constexpr Vec3(const Vec2 vec2) : x(vec2.x), y(vec2.y), z(0.0f) {
	}

	/* ---------------- 関数類 ---------------- */
	/**
	 * @brief ベクトルの長さを取得する
	 * @return ベクトルの長さ
	 */
	float Length() const;

	/**
	 * @brief ベクトルの長さの2乗を取得する
	 * @return ベクトルの長さの2乗
	 */
	float SqrLength() const;

	/**
	 * @brief 他のベクトルとの距離を計算する
	 * @param other 対象ベクトル
	 * @return 2点間の距離
	 */
	float Distance(const Vec3& other) const;

	/**
	 * @brief 内積を計算する
	 * @param other 対象ベクトル
	 * @return 内積の値
	 */
	float Dot(const Vec3& other) const;

	/**
	 * @brief 外積を計算する
	 * @param other 対象ベクトル
	 * @return 外積ベクトル
	 */
	Vec3 Cross(const Vec3& other) const;

	/**
	 * @brief ゼロベクトルかどうかを判定する
	 * @param tolerance 許容誤差（デフォルト: 1e-6f）
	 * @return ゼロベクトルの場合true
	 */
	bool IsZero(float tolerance = 1e-6f) const;

	/**
	 * @brief 他のベクトルと平行かどうかを判定する
	 * @param other 対象ベクトル
	 * @return 平行の場合true
	 */
	bool IsParallel(const Vec3& other) const;

	/**
	 * @brief このベクトルを正規化する
	 */
	void Normalize();

	/**
	 * @brief 正規化されたベクトルを取得する
	 * @return 正規化されたベクトル
	 */
	Vec3 Normalized() const;

	/**
	 * @brief ベクトルを範囲内にクランプする
	 * @param minVec 最小値
	 * @param maxVec 最大値
	 * @return クランプされたベクトル
	 */
	Vec3 Clamp(Vec3 minVec, Vec3 maxVec) const;

	/**
	 * @brief ベクトルの長さを範囲内にクランプする
	 * @param minVec 最小長
	 * @param maxVec 最大長
	 * @return クランプされたベクトル
	 */
	Vec3 ClampLength(float minVec, float maxVec);

	/**
	 * @brief 法線ベクトルに対する反射ベクトルを計算する
	 * @param normal 法線ベクトル
	 * @return 反射ベクトル
	 */
	Vec3 Reflect(const Vec3& normal) const;

	/**
	 * @brief 各成分の絶対値を取得する
	 * @return 絶対値ベクトル
	 */
	Vec3 Abs();

	/**
	 * @brief クォータニオンによる回転を適用する
	 * @param rotation 回転クォータニオン
	 * @return 回転後のベクトル
	 */
	Vec3 TransformDirection(const Quaternion& rotation) const;

	/* ---------------- 演算子 ---------------- */
	float&       operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec3 operator-() const;

	Vec3 operator+(const Vec3& rhs) const;
	Vec3 operator-(const Vec3& rhs) const;
	Vec3 operator*(float rhs) const;
	Vec3 operator/(float rhs) const;

	Vec3 operator+(const float& rhs) const;
	Vec3 operator-(const float& rhs) const;
	Vec3 operator*(const Vec3& rhs) const;
	Vec3 operator/(const Vec3& rhs) const;

	friend Vec3 operator+(float lhs, const Vec3& rhs);
	friend Vec3 operator-(float lhs, const Vec3& rhs);
	friend Vec3 operator*(float lhs, const Vec3& rhs);
	friend Vec3 operator/(float lhs, const Vec3& rhs);

	Vec3& operator+=(const Vec3& rhs);
	Vec3& operator-=(const Vec3& rhs);
	Vec3& operator*=(float rhs);
	Vec3& operator/=(float rhs);

	/* ---------------- その他 ---------------- */
	std::string ToString() const;
	bool        operator!=(const Vec3& rhs) const;
	bool        operator==(const Vec3& vec3) const;

	static Vec3 Min(Vec3 lhs, Vec3 rhs);
	static Vec3 Max(Vec3 lhs, Vec3 rhs);
};
