#include <algorithm>

#include <Lib/Physics/Physics.h>

#define NOMINMAX
#undef min
#undef max

//-----------------------------------------------------------------------------
// Purpose: AABBのコンストラクタ
//-----------------------------------------------------------------------------
Physics::AABB::AABB(const Vec3& min, const Vec3& max) :
	min(min), max(max) {
}

//-----------------------------------------------------------------------------
// Purpose: AABBの中心を取得します
//-----------------------------------------------------------------------------
Vec3 Physics::AABB::GetCenter() const {
	return (min + max) * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: AABBのサイズを取得します
//-----------------------------------------------------------------------------
Vec3 Physics::AABB::GetSize() const {
	return max - min;
}

//-----------------------------------------------------------------------------
// Purpose: AABBの半径を取得します
//-----------------------------------------------------------------------------
Vec3 Physics::AABB::GetHalfSize() const {
	return GetSize() * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: Triangleのコンストラクタ
//-----------------------------------------------------------------------------
Physics::Triangle::Triangle(const Vec3& v0, const Vec3& v1, const Vec3& v2) : v0(v0), v1(v1), v2(v2) {
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の法線ベクトルを取得します
//-----------------------------------------------------------------------------
Vec3 Physics::Triangle::GetNormal() const {
	Vec3 edge0 = v1 - v0;
	Vec3 edge1 = v2 - v0;
	return edge0.Cross(edge1).Normalized();
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の面積を取得します
//-----------------------------------------------------------------------------
float Physics::Triangle::GetArea() const {
	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	return e0.Cross(e1).Length() * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の中心を取得します
//-----------------------------------------------------------------------------
Vec3 Physics::Triangle::GetCenter() const {
	return (v0 + v1 + v2) / 3.0f;
}

bool Physics::Triangle::IsPointInside(const Vec3& point) const {
	// 三角形の3辺と平面法線を利用して点が内部かを判定
	Vec3 normal = (v1 - v0).Cross(v2 - v0).Normalized();

	Vec3 edge0 = v1 - v0;
	Vec3 edge1 = v2 - v1;
	Vec3 edge2 = v0 - v2;

	Vec3 c0 = point - v0;
	Vec3 c1 = point - v1;
	Vec3 c2 = point - v2;

	// 各辺の外積が法線と同じ方向なら内部
	return (edge0.Cross(c0).Dot(normal) >= 0.0f &&
		edge1.Cross(c1).Dot(normal) >= 0.0f &&
		edge2.Cross(c2).Dot(normal) >= 0.0f);
}

Vec3 Physics::Triangle::GetVertex(const int index) const {
	switch (index) {
	case 0:
		return v0;
	case 1:
		return v1;
	case 2:
		return v2;
	default:
		return Vec3::zero;
	}
}

void Physics::ProjectAABBOntoAxis(const Vec3 center, const Vec3 aabbHalfSize, Vec3 axis, float& outMin, float& outMax) {
	// 軸を正規化
	axis.Normalize();

	// AABBの中心点を軸に投影
	float centerProjection = center.Dot(axis);

	// AABBの"半分のサイズ"による投影の範囲を計算
	float radius = std::abs(aabbHalfSize.x * axis.x) +
		std::abs(aabbHalfSize.y * axis.y) +
		std::abs(aabbHalfSize.z * axis.z);

	// 最小値と最大値を計算
	outMin = centerProjection - radius;
	outMax = centerProjection + radius;
}

void Physics::ProjectTriangleOntoAxis(const Triangle& triangle, const Vec3& axis, float& outMin, float& outMax) {
	// 軸を正規化
	Vec3 normalizedAxis = axis.Normalized();

	// 各頂点を軸に投影
	float proj0 = triangle.v0.Dot(normalizedAxis);
	float proj1 = triangle.v1.Dot(normalizedAxis);
	float proj2 = triangle.v2.Dot(normalizedAxis);

	// 最小値と最大値を計算
	outMin = std::min(std::min(proj0, proj1), proj2);
	outMax = std::max(std::max(proj0, proj1), proj2);
}

bool Physics::TestAxis(const Vec3& axis, const Vec3& aabbCenter, const Vec3& aabbHalfSize, const Triangle& triangle) {
	// 軸がゼロの場合は無視
	if (axis.SqrLength() < 1e-6f) {
		return true;
	}

	// AABB軸での範囲
	float aabbMin, aabbMax;
	ProjectAABBOntoAxis(aabbCenter, aabbHalfSize, axis, aabbMin, aabbMax);

	// 三角形軸での範囲
	float triangleMin, triangleMax;
	ProjectTriangleOntoAxis(triangle, axis, triangleMin, triangleMax);

	// 投影範囲が重なっていなければ分離している
	return !(aabbMax < triangleMin || triangleMax < aabbMin);
}

Physics::CollisionResult Physics::IntersectAABBWithTriangle(const AABB& aabb, const Triangle& triangle) {
	// 分離軸アルゴリズムというらしい Separating Axis Theorem
	// 軸の候補を出して、それぞれの軸に対してAABBと三角形を投影して重なっていたら衝突していることになるらしい...?
	// あー
	// そーゆーことね
	// 完全に理解した(わかってない

	CollisionResult result;

	// AABBの中心と半径
	Vec3 aabbCenter = aabb.GetCenter();
	Vec3 aabbExtents = aabb.GetHalfSize();

	// 三角面の辺を求める
	Vec3 e0 = triangle.v1 - triangle.v0;
	Vec3 e1 = triangle.v2 - triangle.v1;
	Vec3 e2 = triangle.v0 - triangle.v2;

	Vec3 x = Vec3::right;
	Vec3 y = Vec3::up;
	Vec3 z = Vec3::forward;

	// AABBの軸で投影
	if (!TestAxis(x, aabbCenter, aabbExtents, triangle)) {
		return result;
	}
	if (!TestAxis(y, aabbCenter, aabbExtents, triangle)) {
		return result;
	}
	if (!TestAxis(z, aabbCenter, aabbExtents, triangle)) {
		return result;
	}

	Vec3 triangleNormal = triangle.GetNormal();

	// 三角形の法線で投影
	if (!TestAxis(triangleNormal, aabbCenter, aabbExtents, triangle)) {
		return result;
	}

	// AABBの辺、三角形の辺のクロスで投影
	Vec3 axes[]{
		x.Cross(e0), x.Cross(e1), x.Cross(e2),
		y.Cross(e0), y.Cross(e1), y.Cross(e2),
		z.Cross(e0), z.Cross(e1), z.Cross(e2)
	};

	for (const Vec3& axis : axes) {
		if (!TestAxis(axis, aabbCenter, aabbExtents, triangle)) {
			return result;
		}
	}

	// 重なっている場合は衝突している
	result.hasCollision = true;
	result.contactNormal = triangleNormal;
	result.contactPoint = aabbCenter - triangleNormal * aabbExtents.Dot(triangleNormal);

	return result;
}

void Physics::DebugDrawAABBAndTriangle(const AABB& aabb, const Triangle& triangle, const CollisionResult& result) {
	// --- 1. AABBを描画 ---
	Vec3 center = (aabb.min + aabb.max) * 0.5f;
	Vec3 size = aabb.max - aabb.min;
	Debug::DrawBox(center, Quaternion(0, 0, 0, 1), size, Vec4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑

	// --- 2. 三角形を描画 ---
	Debug::DrawLine(triangle.v0, triangle.v1, Vec4(0.0f, 0.0f, 1.0f, 1.0f)); // 青
	Debug::DrawLine(triangle.v1, triangle.v2, Vec4(0.0f, 0.0f, 1.0f, 1.0f)); // 青
	Debug::DrawLine(triangle.v2, triangle.v0, Vec4(0.0f, 0.0f, 1.0f, 1.0f)); // 青

	// --- 3. 三角形の法線を描画 ---
	Vec3 triangleCenter = (triangle.v0 + triangle.v1 + triangle.v2) / 3.0f;
	Vec3 triangleNormal = (triangle.v1 - triangle.v0).Cross(triangle.v2 - triangle.v0).Normalized();
	Debug::DrawRay(triangleCenter, triangleNormal * 0.5f, Vec4(1.0f, 1.0f, 0.0f, 1.0f)); // 黄色

	// --- 4. 衝突結果の描画 ---
	if (result.hasCollision) {
		// 衝突点を描画
		Debug::DrawBox(result.contactPoint, Quaternion(0, 0, 0, 1), Vec3(0.1f, 0.1f, 0.1f), Vec4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤

		// 衝突法線を描画
		Debug::DrawRay(result.contactPoint, result.contactNormal * 0.5f, Vec4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤
	}
}

Vec3 Physics::ResolveCollisionAABB(
	const AABB& aabb, Vec3& velocity, const std::vector<Triangle>& triangles, int maxIterations
) {
	Vec3 remainingVelocity = velocity; // 現在の移動ベクトル
	Vec3 positionDelta(0.0f, 0.0f, 0.0f); // 実際の移動量

	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		// 1. 衝突判定
		CollisionResult closestCollision;
		float closestDistance = FLT_MAX;

		for (const auto& triangle : triangles) {
			AABB movedAABB = { aabb.min + positionDelta, aabb.max + positionDelta };
			CollisionResult result = IntersectAABBWithTriangle(movedAABB, triangle);

			// 衝突していて、かつ最も近い衝突点を記録
			if (result.hasCollision && result.distance < closestDistance) {
				closestCollision = result;
				closestDistance = result.distance;
			}
		}

		// 2. 衝突がない場合、残りの移動を適用して終了
		if (!closestCollision.hasCollision) {
			positionDelta += remainingVelocity;
			break;
		}

		// 3. 衝突がある場合、スライド移動を計算
		Vec3 slideNormal = closestCollision.contactNormal;
		Vec3 slideVelocity = remainingVelocity - remainingVelocity.Dot(slideNormal) * slideNormal;

		velocity = slideVelocity;

		// 実際に移動する量を更新
		positionDelta += remainingVelocity * (closestCollision.distance - 0.01f); // 0.01fは浮動小数点誤差を防ぐマージン
		remainingVelocity = slideVelocity;

		// 4. 移動量が小さくなったら終了
		if (remainingVelocity.SqrLength() < 1e-6f) {
			break;
		}
	}

	return positionDelta;
}

void Physics::DebugDrawAABBCollisionResponse(const AABB& aabb, const Vec3& velocity, const Vec3& finalPosition) {
	// 初期位置のAABB
	Vec3 initialCenter = (aabb.min + aabb.max) * 0.5f;
	Vec3 finalCenter = initialCenter + finalPosition;

	Debug::DrawBox(initialCenter, Quaternion::identity, aabb.max - aabb.min, Vec4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑
	Debug::DrawBox(finalCenter, Quaternion::identity, aabb.max - aabb.min, Vec4(0.0f, 0.0f, 1.0f, 1.0f)); // 青

	// 移動経路の描画
	Debug::DrawRay(initialCenter, velocity, Vec4(1.0f, 1.0f, 0.0f, 1.0f)); // 黄色い線で移動経路を描画
}

void Physics::ClosestPointBetweenSegments(
	const Vec3& segA1, const Vec3& segA2, const Vec3& segB1, const Vec3& segB2, Vec3& outSegA, Vec3& outSegB
) {
	Vec3 d1 = segA2 - segA1; // 線分Aの方向ベクトル
	Vec3 d2 = segB2 - segB1; // 線分Bの方向ベクトル
	Vec3 r = segA1 - segB1;

	float a = d1.Dot(d1); // 線分Aの長さの2乗
	float e = d2.Dot(d2); // 線分Bの長さの2乗
	float f = d2.Dot(r);

	float s, t;

	// 線分Aがデジェネレート（点）でない場合
	if (a > 1e-6f) {
		float c = d1.Dot(r);
		float b = d1.Dot(d2);
		float denom = a * e - b * b; // 並行性をチェック

		// 線分が並行でない場合
		if (std::abs(denom) > 1e-6f) {
			s = (b * f - c * e) / denom;
			s = std::clamp(s, 0.0f, 1.0f);
		} else {
			s = 0.0f;
		}
	} else {
		s = 0.0f; // 線分Aが点
	}

	// 線分Bがデジェネレート（点）でない場合
	if (e > 1e-6f) {
		t = (d2.Dot(r) + s * d1.Dot(d2)) / e;
		t = std::clamp(t, 0.0f, 1.0f);
	} else {
		t = 0.0f; // 線分Bが点
	}

	// 最近接点を計算
	outSegA = segA1 + d1 * s;
	outSegB = segB1 + d2 * t;
}

Vec3 Physics::ClosestPointOnTriangle(const Vec3& point, const Triangle& triangle) {
	Vec3 ab = triangle.v1 - triangle.v0;
	Vec3 ac = triangle.v2 - triangle.v0;
	Vec3 ap = point - triangle.v0;

	float d1 = ab.Dot(ap);
	float d2 = ac.Dot(ap);

	if (d1 <= 0.0f && d2 <= 0.0f) return triangle.v0; // 最も近い頂点はv0

	Vec3 bp = point - triangle.v1;
	float d3 = ab.Dot(bp);
	float d4 = ac.Dot(bp);

	if (d3 >= 0.0f && d4 <= d3) return triangle.v1; // 最も近い頂点はv1

	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		float v = d1 / (d1 - d3);
		return triangle.v0 + ab * v; // v0-v1 エッジ上の最近接点
	}

	Vec3 cp = point - triangle.v2;
	float d5 = ab.Dot(cp);
	float d6 = ac.Dot(cp);

	if (d6 >= 0.0f && d5 <= d6) return triangle.v2; // 最も近い頂点はv2

	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		float w = d2 / (d2 - d6);
		return triangle.v0 + ac * w; // v0-v2 エッジ上の最近接点
	}

	float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return triangle.v1 + (triangle.v2 - triangle.v1) * w; // v1-v2 エッジ上の最近接点
	}

	// 面の内部
	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;
	return triangle.v0 + ab * v + ac * w;
}

Physics::CollisionResult Physics::IntersectCapsuleWithTriangle(const Capsule& capsule, const Triangle& triangle) {
	CollisionResult result;

	// カプセルの線分
	Vec3 capsuleStart = capsule.start;
	Vec3 capsuleEnd = capsule.end;

	// 三角形の法線
	Vec3 triangleNormal = triangle.GetNormal();

	// --- 1. カプセル線分と三角形の平面との交差をチェック ---
	// 平面との距離を計算
	float startDist = (capsuleStart - triangle.v0).Dot(triangleNormal);
	float endDist = (capsuleEnd - triangle.v0).Dot(triangleNormal);

	// 線分が平面を跨いでいるか、または端点が平面上にあるかをチェック
	if (startDist * endDist <= 0.0f || std::abs(startDist) <= capsule.radius || std::abs(endDist) <= capsule.radius) {
		// 線分と平面の交点を計算
		Vec3 dir = capsuleEnd - capsuleStart;
		float denom = dir.Dot(triangleNormal);

		// 線分が平面と平行でない場合のみ計算
		if (std::abs(denom) > 1e-6f) {
			float t = ((triangle.v0 - capsuleStart).Dot(triangleNormal)) / denom;
			t = std::clamp(t, 0.0f, 1.0f);
			Vec3 planeIntersection = capsuleStart + dir * t;

			// 交点が三角形内にあるかチェック
			if (triangle.IsPointInside(planeIntersection)) {
				result.hasCollision = true;
				result.contactPoint = planeIntersection - triangleNormal * capsule.radius;
				result.contactNormal = triangleNormal;
				result.distance = 0.0f; // 平面上での衝突なので距離は0
				return result;
			}
		}
	}

	// --- 2. カプセル線分と三角形の辺との最近接点をチェック ---
	float minDistance = FLT_MAX;
	Vec3 closestCapsulePoint;
	Vec3 closestTriangleEdgePoint;

	for (int i = 0; i < 3; ++i) {
		Vec3 edgeStart = triangle.GetVertex(i);
		Vec3 edgeEnd = triangle.GetVertex((i + 1) % 3);

		Vec3 tempCapsulePoint, tempEdgePoint;
		ClosestPointBetweenSegments(capsuleStart, capsuleEnd, edgeStart, edgeEnd, tempCapsulePoint, tempEdgePoint);

		float dist = (tempCapsulePoint - tempEdgePoint).Length();
		if (dist < minDistance) {
			minDistance = dist;
			closestCapsulePoint = tempCapsulePoint;
			closestTriangleEdgePoint = tempEdgePoint;
		}
	}

	// --- 3. カプセルの端点と三角形の面との最近接点をチェック ---
	Vec3 closestPointOnTriangleToStart = ClosestPointOnTriangle(capsuleStart, triangle);
	float distToStart = (capsuleStart - closestPointOnTriangleToStart).Length();
	if (distToStart < minDistance) {
		minDistance = distToStart;
		closestCapsulePoint = capsuleStart;
		closestTriangleEdgePoint = closestPointOnTriangleToStart;
	}

	Vec3 closestPointOnTriangleToEnd = ClosestPointOnTriangle(capsuleEnd, triangle);
	float distToEnd = (capsuleEnd - closestPointOnTriangleToEnd).Length();
	if (distToEnd < minDistance) {
		minDistance = distToEnd;
		closestCapsulePoint = capsuleEnd;
		closestTriangleEdgePoint = closestPointOnTriangleToEnd;
	}

	// --- 4. 衝突判定 ---
	if (minDistance <= capsule.radius) {
		result.hasCollision = true;
		result.contactPoint = closestCapsulePoint - (closestCapsulePoint - closestTriangleEdgePoint).Normalized() * capsule.radius;
		result.contactNormal = (closestCapsulePoint - closestTriangleEdgePoint).Normalized();
		result.distance = capsule.radius - minDistance;
	}

	return result;
}

void Physics::DebugDrawCapsuleAndTriangle(
	const Capsule& capsule, const Triangle& triangle, const CollisionResult& result
) {
	// --- 1. カプセルを描画 ---
	Debug::DrawLine(capsule.start, capsule.end, Vec4(0.0f, 1.0f, 1.0f, 1.0f));
	Debug::DrawCapsule(capsule.start, capsule.end, capsule.radius, Vec4(0.0f, 1.0f, 1.0f, 0.5f));

	// --- 2. 三角形を描画 ---
	Debug::DrawLine(triangle.v0, triangle.v1, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	Debug::DrawLine(triangle.v1, triangle.v2, Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	Debug::DrawLine(triangle.v2, triangle.v0, Vec4(0.0f, 0.0f, 1.0f, 1.0f));

	Debug::DrawRay(triangle.GetCenter(), triangle.GetNormal() * 0.5f, Vec4(1.0f, 1.0f, 0.0f, 1.0f));

	// --- 3. 衝突点を描画 ---
	if (result.hasCollision) {
		// カプセル線分上の最近接点（黄）
		Vec3 closestSegPoint = result.contactPoint + result.contactNormal * capsule.radius;
		Debug::DrawBox(closestSegPoint, Quaternion::identity, Vec3(0.1f, 0.1f, 0.1f), Vec4(1.0f, 1.0f, 0.0f, 1.0f));

		// 実際の接触点（赤）
		Debug::DrawBox(result.contactPoint, Quaternion::identity, Vec3(0.1f, 0.1f, 0.1f), Vec4(1.0f, 0.0f, 0.0f, 1.0f));

		// 衝突法線（赤）
		Debug::DrawRay(result.contactPoint, result.contactNormal * 0.5f, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	}
}
