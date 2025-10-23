#pragma once

#include <initializer_list>
#include <string>

struct Quaternion;

/**
 * @brief 4x4行列構造体
 */
struct Mat4 final {
	float m[4][4]; ///< 行列の要素（4x4）

	/**
	 * @brief デフォルトコンストラクタ
	 */
	Mat4();

	/**
	 * @brief コピーコンストラクタ
	 * @param other コピー元の行列
	 */
	Mat4(const Mat4& other);

	/**
	 * @brief 初期化リストから生成するコンストラクタ
	 * @param list 2次元初期化リスト
	 */
	Mat4(std::initializer_list<std::initializer_list<float>> list);

	static const Mat4 identity; ///< 単位行列
	static const Mat4 zero;     ///< ゼロ行列

	//-------------------------------------------------------------------------
	// Functions
	//-------------------------------------------------------------------------
	/**
	 * @brief 逆行列を取得する
	 * @return 逆行列
	 */
	[[nodiscard]] Mat4 Inverse() const;

	/**
	 * @brief 転置行列を取得する
	 * @return 転置行列
	 */
	[[nodiscard]] Mat4 Transpose() const;

	/**
	 * @brief 平行移動行列を生成する
	 * @param translate 移動量
	 * @return 平行移動行列
	 */
	static Mat4 Translate(const Vec3& translate);

	/**
	 * @brief スケール行列を生成する
	 * @param scale スケール値
	 * @return スケール行列
	 */
	static Mat4 Scale(const Vec3& scale);

	/**
	 * @brief ベクトルを行列で変換する
	 * @param vector 変換するベクトル
	 * @param matrix 変換行列
	 * @return 変換されたベクトル
	 */
	static Vec3 Transform(const Vec3& vector, const Mat4& matrix);

	/**
	 * @brief クォータニオンから回転行列を生成する
	 * @param quaternion 回転クォータニオン
	 * @return 回転行列
	 */
	static Mat4 RotateQuaternion(Quaternion quaternion);

	/**
	 * @brief クォータニオンから行列を生成する
	 * @param q クォータニオン
	 * @return 回転行列
	 */
	static Mat4 FromQuaternion(const Quaternion& q);

	/**
	 * @brief X軸周りの回転行列を生成する
	 * @param radian 回転角（ラジアン）
	 * @return 回転行列
	 */
	static Mat4 RotateX(float radian);

	/**
	 * @brief Y軸周りの回転行列を生成する
	 * @param radian 回転角（ラジアン）
	 * @return 回転行列
	 */
	static Mat4 RotateY(float radian);

	/**
	 * @brief Z軸周りの回転行列を生成する
	 * @param radian 回転角（ラジアン）
	 * @return 回転行列
	 */
	static Mat4 RotateZ(float radian);

	/**
	 * @brief アフィン変換行列を生成する
	 * @param scale スケール
	 * @param rotate 回転（オイラー角）
	 * @param translate 平行移動
	 * @return アフィン変換行列
	 */
	static Mat4 Affine(const Vec3& scale, const Vec3& rotate,
	                   const Vec3& translate);

	/**
	 * @brief アフィン変換行列を生成する
	 * @param scale スケール
	 * @param rotate 回転（クォータニオン）
	 * @param translate 平行移動
	 * @return アフィン変換行列
	 */
	static Mat4 Affine(const Vec3& scale, const Quaternion& rotate,
	                   const Vec3& translate);

	/**
	 * @brief 透視投影行列を生成する
	 * @param fovY 垂直視野角（ラジアン）
	 * @param aspectRatio アスペクト比
	 * @param nearClip 近クリップ面
	 * @param farClip 遠クリップ面
	 * @return 透視投影行列
	 */
	static Mat4 PerspectiveFovMat(float fovY, float aspectRatio, float nearClip,
	                              float farClip);

	/**
	 * @brief 正射影行列を生成する
	 * @param left 左端
	 * @param top 上端
	 * @param right 右端
	 * @param bottom 下端
	 * @param nearClip 近クリップ面
	 * @param farClip 遠クリップ面
	 * @return 正射影行列
	 */
	static Mat4 MakeOrthographicMat(float left, float   top, float right,
	                                float bottom, float nearClip,
	                                float farClip);

	/**
	 * @brief ビューポート行列を生成する
	 * @param left 左端
	 * @param top 上端
	 * @param width 幅
	 * @param height 高さ
	 * @param minDepth 最小深度
	 * @param maxDepth 最大深度
	 * @return ビューポート行列
	 */
	static Mat4 ViewportMat(float left, float top, float width, float height,
	                        float minDepth, float maxDepth);

	/**
	 * @brief 行列をクォータニオンに変換する
	 * @return クォータニオン
	 */
	Quaternion ToQuaternion() const;

	/**
	 * @brief 点を変換する
	 * @param vec3 変換する点
	 * @return 変換された点
	 */
	Vec3& TransformPoint(Vec3 vec3) const;

	/**
	 * @brief 平行移動成分を取得する
	 * @return 平行移動ベクトル
	 */
	Vec3 GetTranslate();

	/**
	 * @brief 回転成分を取得する
	 * @return 回転ベクトル（オイラー角）
	 */
	Vec3 GetRotate() const;

	/**
	 * @brief スケール成分を取得する
	 * @return スケールベクトル
	 */
	Vec3 GetScale() const;

	/**
	 * @brief 右方向ベクトルを取得する
	 * @return 右方向ベクトル
	 */
	Vec3 GetRight();

	/**
	 * @brief 上方向ベクトルを取得する
	 * @return 上方向ベクトル
	 */
	Vec3 GetUp();

	/**
	 * @brief 前方向ベクトルを取得する
	 * @return 前方向ベクトル
	 */
	Vec3 GetForward();

	//-------------------------------------------------------------------------
	// Operator
	//-------------------------------------------------------------------------
	Mat4 operator+(const Mat4& rhs) const;
	Mat4 operator-(const Mat4& rhs) const;
	Mat4 operator*(const Mat4& rhs) const;
	Vec4 operator*(const Vec4& vec) const;

	Mat4& operator*=(const Mat4& mat4);

	bool operator==(const Mat4& mat4) const;

private:
	float Determinant() const;
};
