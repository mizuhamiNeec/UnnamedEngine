#pragma once
#include <cstdint>

/// @brief 2次元ベクトル構造体
struct Vec2 final {
	float x, y;

	static const Vec2 zero;  /// ゼロベクトル (0, 0)
	static const Vec2 one;   /// 単位ベクトル (1, 1)
	static const Vec2 right; /// 右方向 (1, 0)
	static const Vec2 left;  /// 左方向 (-1, 0)
	static const Vec2 up;    /// 上方向 (0, 1)
	static const Vec2 down;  /// 下方向 (0, -1)


	/// @brief コンストラクタ
	/// @param x X成分（デフォルト: 0.0f）
	/// @param y Y成分（デフォルト: 0.0f）
	constexpr Vec2(const float x = 0.0f, const float y = 0.0f) : x(x), y(y) {
	}

	/* ---------------- 関数類 ---------------- */


	///  @brief ベクトルの長さを取得する
	///  @return ベクトルの長さ
	[[nodiscard]] float Length() const;


	///  @brief ベクトルの長さの2乗を取得する
	///  @return ベクトルの長さの2乗
	[[nodiscard]] float SqrLength() const;


	///  @brief 他のベクトルとの距離を計算する
	///  @param other 対象ベクトル
	///  @return 2点間の距離
	[[nodiscard]] float Distance(const Vec2& other) const;


	///  @brief 内積を計算する
	///  @param other 対象ベクトル
	///  @return 内積の値
	[[nodiscard]] float Dot(const Vec2& other) const;


	///  @brief 外積を計算する
	///  @param other 対象ベクトル
	///  @return 外積の値
	[[nodiscard]] float Cross(const Vec2& other) const;


	///  @brief ゼロベクトルかどうかを判定する
	///  @param tolerance 許容誤差（デフォルト: 1e-6f）
	///  @return ゼロベクトルの場合true
	[[nodiscard]] bool IsZero(float tolerance = 1e-6f) const;


	///  @brief このベクトルを正規化する
	void Normalize();


	///  @brief 正規化されたベクトルを取得する
	///  @return 正規化されたベクトル
	[[nodiscard]] Vec2 Normalized() const;


	///  @brief ベクトルを範囲内にクランプする
	///  @param min 最小値
	///  @param max 最大値
	///  @return クランプされたベクトル
	[[nodiscard]] Vec2 Clamp(Vec2 min, Vec2 max) const;


	///  @brief ベクトルの長さを範囲内にクランプする
	///  @param min 最小長
	///  @param max 最大長
	///  @return クランプされたベクトル
	Vec2 ClampLength(float min, float max);

	///   @brief 線形補間を行います
	///   @param target 目標ベクトル
	///   @param t 補間係数（0.0～1.0）
	///   @return 補間されたベクトル
	[[nodiscard]] Vec2 Lerp(const Vec2& target, float t) const;

	///   @brief 法線ベクトルに対する反射ベクトルを計算する
	///   @param normal 法線ベクトル
	///   @return 反射ベクトル
	[[nodiscard]] Vec2 Reflect(const Vec2& normal) const;


	///  @brief ベクトルを回転する
	///  @param angleZ Z軸周りの回転角（ラジアン）
	///  @return 回転後のベクトル
	[[nodiscard]] Vec2 RotateVector(float angleZ) const;

	/* ---------------- 演算子 ---------------- */

	float&       operator[](uint32_t index);
	const float& operator[](uint32_t index) const;

	Vec2 operator-() const;

	Vec2 operator+(const Vec2& rhs) const;
	Vec2 operator-(const Vec2& rhs) const;
	Vec2 operator*(float rhs) const;
	Vec2 operator/(float rhs) const;

	Vec2 operator+(const float& rhs) const;
	Vec2 operator-(const float& rhs) const;
	Vec2 operator*(const Vec2& rhs) const;
	Vec2 operator/(const Vec2& rhs) const;

	friend Vec2 operator+(float lhs, const Vec2& rhs);
	friend Vec2 operator-(float lhs, const Vec2& rhs);
	friend Vec2 operator*(float lhs, const Vec2& rhs);
	friend Vec2 operator/(float lhs, const Vec2& rhs);

	Vec2& operator+=(const Vec2& rhs);
	Vec2& operator-=(const Vec2& rhs);
	Vec2& operator*=(float rhs);
	Vec2& operator/=(float rhs);
};
