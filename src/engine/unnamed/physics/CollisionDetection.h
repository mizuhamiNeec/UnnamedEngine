#pragma once

struct Vec3;

namespace Unnamed {
	struct Box;
	struct Triangle;
	struct AABB;
	struct Ray;
}

namespace Unnamed::Physics {
	/// @brief 三角形上で指定点に最も近い点を返します。
	/// @param tri 三角形
	/// @param point 判定する点
	/// @return 三角形上の最近点
	Vec3 ClosestPointOnTriangle(const Triangle& tri, const Vec3& point);

	/// @brief レイとAABBの交差判定を行います
	/// @param ray 判定するレイ
	/// @param aabb 判定するAABB
	/// @param tMaxOut レイがAABBに衝突するまでの距離を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	);

	/// @brief レイと三角形の交差判定を行います（Möller–Trumboreアルゴリズム）
	/// @param triangle 判定する三角形
	/// @param ray 判定するレイ
	/// @param tHit 衝突点までの距離を格納する出力変数
	/// @param outNormal 衝突点の法線を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	);

	/// @brief スイープAABBと三角形の連続衝突判定を行います（SAT）
	/// @param box0 判定するAABBの初期位置
	/// @param delta AABBの移動ベクトル
	/// @param tri 判定する三角形
	/// @param outTOI 衝突までの時間を格納する出力変数（0.0〜1.0）
	/// @param outNrm 衝突時の法線を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool SweptAabbVsTriSAT(
		const Box&      box0,
		const Vec3&     delta, // 速度 * dt
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	);

	/// @brief スイープ球と三角形の連続衝突判定を行います
	/// @param center 判定する球の中心位置
	/// @param radius 球の半径
	/// @param delta 球の移動ベクトル
	/// @param tri 判定する三角形
	/// @param outTOI 衝突までの時間を格納する出力変数（0.0〜1.0）
	/// @param outNormal 衝突時の法線を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool SweptSphereVsTriSAT(
		const Vec3&     center,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	/// @brief スイープ円柱と三角形の連続衝突判定を行います（SAT）
	/// @param base 円柱の底面中心位置
	/// @param halfHeight 円柱の半高さ
	/// @param radius 円柱の半径
	/// @param delta 円柱の移動ベクトル
	/// @param tri 判定する三角形
	/// @param outTOI 衝突までの時間を格納する出力変数（0.0〜1.0）
	/// @param outNormal 衝突時の法線を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool SweptCylinderVsTriSAT(
		const Vec3&     base,
		float           halfHeight,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	/// @brief スイープカプセルと三角形の連続衝突判定を行います（SAT）
	/// @param a カプセルの一端点位置
	/// @param b カプセルの他端点位置
	/// @param radius カプセルの半径
	/// @param delta カプセルの移動ベクトル
	/// @param tri 判定する三角形
	/// @param outTOI 衝突までの時間を格納する出力変数（0.0〜1.0）
	/// @param outNormal 衝突時の法線を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool SweptCapsuleVsTriSAT(
		const Vec3&     a,
		const Vec3&     b,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	/// @brief ボックスと三角形の静的衝突判定を行います（SAT）
	/// @param box 判定するボックス
	/// @param tri 判定する三角形
	/// @param outNormal 衝突時の法線を格納する出力変数
	/// @param outDepth 衝突の貫入深度を格納する出力変数
	/// @return 衝突する場合はtrue、しない場合はfalse
	bool BoxVsTriangleOverlap(
		const Box&      box,
		const Triangle& tri,
		Vec3&           outNormal,
		float&          outDepth
	);
}
