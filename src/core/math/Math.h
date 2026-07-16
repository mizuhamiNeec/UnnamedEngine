#pragma once

#include <numbers>

#include <core/math/Vec2.h>
#include <core/math/Vec3.h>
#include <core/math/Vec4.h>

namespace Math {
	//-------------------------------------------------------------------------
	// 定数
	//-------------------------------------------------------------------------
	constexpr float pi = std::numbers::pi_v<float>; // π

	constexpr float eps = std::numeric_limits<float>::epsilon(); // 浮動小数点の最小値

	constexpr float deg2Rad = pi / 180.0f; // 度からラジアンへの変換
	constexpr float rad2Deg = 180.0f / pi; // ラジアンから度への変換

	//-------------------------------------------------------------------------
	// 関数
	//-------------------------------------------------------------------------
	/// @brief 線形補間を行います。
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	float Lerp(float a, float b, float t);

	/// @brief 2つの角度の最短符号付き差分を計算します。
	/// @param current 現在の角度 [rad]
	/// @param target 目標の角度 [rad]
	/// @return [-π, π]に正規化された角度差 [rad]
	float DeltaAngle(float current, float target);

	/// @brief 2つの度数角の最短符号付き差分を計算します。
	/// @param current 現在の角度 [deg]
	/// @param target 目標の角度 [deg]
	/// @return [-180, 180]に正規化された角度差 [deg]
	float DeltaAngleDegrees(float current, float target);

	/// @brief 3次ベジェ曲線を計算します。
	/// @param t 補間パラメータ [0, 1]
	/// @param p1 始点
	/// @param p2 終点
	/// @return 補間結果
	float CubicBezier(float t, Vec2 p1, Vec2 p2);

	/// @brief 3次ベジェ曲線を計算します。
	/// @param t 補間パラメータ [0, 1]
	/// @param p1 制御点1のx成分
	/// @param p2 制御点1のy成分
	/// @param p3 制御点2のx成分
	/// @param p4 制御点2のy成分
	/// @return 補間結果
	float CubicBezier(float t, float p1, float p2, float p3, float p4);

	// Vec2
	/// @brief 線形補間を行います。
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	Vec2 Lerp(const Vec2& a, const Vec2& b, float t);

	/// @brief ワールド座標をスクリーン座標に変換します。
	/// @param worldPos 変換するワールド座標
	/// @param screenSize 画面サイズ
	/// @param bClamp 画面外の座標を画面端にクランプするか?
	/// @param margin 画面端からのマージン[px]
	/// @param outIsOffscreen 画面外にあるか?の結果
	/// @param outAngle 画面中心からの角度 [rad]
	/// @return スクリーン座標
	Vec2 WorldToScreen(
		const Vec3& worldPos, Vec2         screenSize,
		const bool& bClamp, const float&   margin,
		bool&       outIsOffscreen, float& outAngle
	);

	// Vec3
	/// @brief ベクトルを平面に射影します。
	/// @param vector 射影するベクトル
	/// @param normal 平面の法線ベクトル
	/// @return 射影後のベクトル
	Vec3 ProjectOnPlane(const Vec3& vector, const Vec3& normal);

	/// @brief 地面の法線に基づいて移動方向を取得します。
	/// @param forward 移動したい方向のベクトル
	/// @param groundNormal 地面の法線ベクトル
	/// @return 地面に沿った移動方向のベクトル
	Vec3 GetMoveDirection(const Vec3& forward, const Vec3& groundNormal);

	/// @brief 線形補間を行います。
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	Vec3 Lerp(const Vec3& a, const Vec3& b, float t);

	/// @brief 各成分ごとの最小値を取得します。
	/// @param a ベクトルA
	/// @param b ベクトルB
	/// @return 各成分ごとの最小値を持つベクトル
	Vec3 Min(Vec3 a, Vec3 b);
	/// @brief 各成分ごとの最大値を取得します。
	/// @param a ベクトルA
	/// @param b ベクトルB
	Vec3 Max(Vec3 a, Vec3 b);

	// Vec4
	/// @brief 線形補間を行います。
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	Vec4 Lerp(const Vec4& a, const Vec4& b, float t);

	//-------------------------------------------------------------------------
	// 単位変換 (メートル <-> Hammer Units(インチ))
	//-------------------------------------------------------------------------
	Vec3  HtoM(const Vec3& vec);
	float HtoM(float val);
	Vec3  MtoH(const Vec3& vec);
	float MtoH(float val);
}
