#pragma once

struct Vec3;

/**
 * @brief クォータニオン（四元数）構造体
 */
struct Quaternion {
	float x, y, z, w;

	static const Quaternion identity; ///< 単位クォータニオン（回転なし）

	/**
	 * @brief デフォルトコンストラクタ
	 */
	Quaternion();

	/**
	 * @brief コンストラクタ
	 * @param x X成分
	 * @param y Y成分
	 * @param z Z成分
	 * @param w W成分
	 */
	Quaternion(float x, float y, float z, float w);

	/**
	 * @brief 軸と角度から生成するコンストラクタ
	 * @param axis 回転軸
	 * @param angleRad 回転角（ラジアン）
	 */
	Quaternion(const Vec3& axis, float angleRad);

	//-------------------------------------------------------------------------
	// Func
	//-------------------------------------------------------------------------
	/**
	 * @brief このクォータニオンを正規化する
	 */
	void Normalize();

	/**
	 * @brief 正規化されたクォータニオンを取得する
	 * @return 正規化されたクォータニオン
	 */
	Quaternion Normalized() const;

	/**
	 * @brief 共役クォータニオンを取得する
	 * @return 共役クォータニオン
	 */
	Quaternion Conjugate() const;

	/**
	 * @brief 逆クォータニオンを取得する
	 * @return 逆クォータニオン
	 */
	Quaternion Inverse() const;

	/**
	 * @brief 軸と角度に変換する
	 * @param outAxis 出力：回転軸
	 * @param outAngle 出力：回転角
	 */
	void ToAxisAngle(Vec3& outAxis, float& outAngle) const;

	/**
	 * @brief オイラー角（ラジアン）からクォータニオンを生成する
	 * @param eulerRad オイラー角（ラジアン）
	 * @return クォータニオン
	 */
	static Quaternion Euler(const Vec3& eulerRad);

	/**
	 * @brief オイラー角（ラジアン）からクォータニオンを生成する
	 * @param x X軸回転（ラジアン）
	 * @param y Y軸回転（ラジアン）
	 * @param z Z軸回転（ラジアン）
	 * @return クォータニオン
	 */
	static Quaternion Euler(const float& x, const float& y, const float& z);

	/**
	 * @brief オイラー角（度数法）からクォータニオンを生成する
	 * @param eulerDeg オイラー角（度数法）
	 * @return クォータニオン
	 */
	static Quaternion EulerDegrees(const Vec3& eulerDeg);

	/**
	 * @brief オイラー角（度数法）からクォータニオンを生成する
	 * @param x X軸回転（度数法）
	 * @param y Y軸回転（度数法）
	 * @param z Z軸回転（度数法）
	 * @return クォータニオン
	 */
	static Quaternion EulerDegrees(const float& x, const float& y,
	                               const float& z);

	/**
	 * @brief 軸と角度からクォータニオンを生成する
	 * @param axis 回転軸
	 * @param angleDeg 回転角（度数法）
	 * @return クォータニオン
	 */
	static Quaternion AxisAngle(const Vec3& axis, const float& angleDeg);

	/**
	 * @brief 前方向と上方向から回転を生成する
	 * @param forward 前方向ベクトル
	 * @param up 上方向ベクトル（デフォルト: Vec3::up）
	 * @return クォータニオン
	 */
	static Quaternion LookRotation(const Vec3& forward,
	                               const Vec3& up = Vec3::up);

	/**
	 * @brief 線形補間を行う
	 * @param a 開始クォータニオン
	 * @param b 終了クォータニオン
	 * @param t 補間係数（0.0～1.0）
	 * @return 補間されたクォータニオン
	 */
	static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);

	/**
	 * @brief 球面線形補間を行う
	 * @param a 開始クォータニオン
	 * @param b 終了クォータニオン
	 * @param t 補間係数（0.0～1.0）
	 * @return 補間されたクォータニオン
	 */
	static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

	/**
	 * @brief オイラー角（ラジアン）に変換する
	 * @return オイラー角（ラジアン）
	 */
	Vec3 ToEulerAngles() const;

	/**
	 * @brief オイラー角（度数法）に変換する
	 * @return オイラー角（度数法）
	 */
	Vec3 ToEulerDegrees() const;

	/**
	 * @brief 回転軸を取得する
	 * @return 回転軸ベクトル
	 */
	Vec3 GetAxis() const;

	/**
	 * @brief ベクトルを回転する
	 * @param vec3 対象ベクトル
	 * @return 回転後のベクトル
	 */
	Vec3 RotateVector(Vec3 vec3) const;

	/**
	 * @brief 指定軸周りの回転角を取得する
	 * @param axis 回転軸
	 * @return 回転角
	 */
	float GetRotationAroundAxis(const Vec3& axis) const;

	/**
	 * @brief 回転角（ラジアン）を取得する
	 * @return 回転角（ラジアン）
	 */
	float GetAngle() const;

	/**
	 * @brief 回転角（度数法）を取得する
	 * @return 回転角（度数法）
	 */
	float GetAngleDegrees() const;

	//-------------------------------------------------------------------------
	// Operator
	//-------------------------------------------------------------------------
	Quaternion operator*(const Quaternion& other) const;
	Vec3       operator*(const Vec3& vec) const;
};
