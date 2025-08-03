#include <engine/public/uphysics/CollisionDetection.h>

#include <algorithm>

namespace UPhysics {
	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	) {
		float tMin = ray.tMin;
		tMaxOut    = ray.tMax;

		for (int i = 0; i < 3; ++i) {
			if (fabs(ray.dir[i]) < 1e-8f) {
				if (ray.start[i] < aabb.min[i] || ray.start[i] > aabb.max[i])
					return false;
			} else {
				const float invD = 1.0f / ray.dir[i];
				float       t1   = (aabb.min[i] - ray.start[i]) * invD;
				float       t2   = (aabb.max[i] - ray.start[i]) * invD;
				if (t1 > t2) std::swap(t1, t2);
				tMin    = t1 > tMin ? t1 : tMin;
				tMaxOut = t2 < tMaxOut ? t2 : tMaxOut;
				if (tMin > tMaxOut)
					return false;
			}
		}
		return true;
	}

	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	) {
		constexpr float kEpsilon = 1e-6f;
		const Vec3      e1       = triangle.v1 - triangle.v0;
		const Vec3      e2       = triangle.v2 - triangle.v0;
		const Vec3      p        = ray.dir.Cross(e2);
		const float     det      = e1.Dot(p);

		if (fabs(det) < kEpsilon) {
			return false;
		}
		float invDet = 1.0f / det;

		Vec3  s = ray.start - triangle.v0;
		float u = s.Dot(p) * invDet;
		if (u < 0.0f || u > 1.0f) {
			return false;
		}

		Vec3  q = s.Cross(e1);
		float v = ray.dir.Dot(q) * invDet;
		if (v < 0.0f || u + v > 1.0f) {
			return false;
		}

		float t = e2.Dot(q) * invDet;
		if (t < ray.tMin || t > tHit) {
			return false; // レイは三角形と交差しない
		}

		tHit      = t;
		outNormal = e1.Cross(e2).Normalized();

		return true;
	}

	bool SweptAabbVsTriSAT(
		const Box&      box0,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	) {
		/* ---------- 1. 基本量 ---------- */
		const Vec3 C0 = box0.center;
		const Vec3 H  = box0.half;
		const Vec3 V  = delta; // 世界ベロシティ

		/* ---------- 2. 13 軸を列挙 ---------- */
		Vec3 axes[13];
		int  axisCount = 0;

		/* 2-1 三角形法線 */
		axes[axisCount++] =
			(tri.v1 - tri.v0).Cross(tri.v2 - tri.v0).Normalized();

		/* 2-2 ボックス軸 (世界 XYZ) */
		axes[axisCount++] = {1, 0, 0};
		axes[axisCount++] = {0, 1, 0};
		axes[axisCount++] = {0, 0, 1};

		/* 2-3 triEdge × boxAxis (交叉積) */
		Vec3 e0          = tri.v1 - tri.v0;
		Vec3 e1          = tri.v2 - tri.v1;
		Vec3 e2          = tri.v0 - tri.v2;
		Vec3 triEdges[3] = {e0, e1, e2};
		for (Vec3 e : triEdges) {
			if (e.Dot(e) < 1e-10f) continue; // degenerate
			Vec3 n0 = e.Cross(Vec3::right).Normalized();
			Vec3 n1 = e.Cross(Vec3::up).Normalized();
			Vec3 n2 = e.Cross(Vec3::forward).Normalized();
			if (n0.Dot(n0) > 1e-6f) axes[axisCount++] = n0;
			if (n1.Dot(n1) > 1e-6f) axes[axisCount++] = n1;
			if (n2.Dot(n2) > 1e-6f) axes[axisCount++] = n2;
		}

		float tEnter = 0.0f; // global [0,1]
		float tExit  = 1.0f;
		Vec3  bestNrm{0, 0, 0};

		/* ---------- 3. 各軸で連続衝突テスト ---------- */
		for (int a = 0; a < axisCount; ++a) {
			Vec3 A = axes[a];
			if (A.Dot(A) < 1e-8f) continue; // 0 ベクトルはスキップ

			/* ボックス：中心投影 ± radius */
			float projC0 = C0.Dot(A);
			float rBox = fabsf(A.x) * H.x + fabsf(A.y) * H.y + fabsf(A.z) * H.z;
			float minBox0 = projC0 - rBox;
			float maxBox0 = projC0 + rBox;

			/* 三角形：3頂点投影 min/max */
			float v0     = tri.v0.Dot(A);
			float v1     = tri.v1.Dot(A);
			float v2     = tri.v2.Dot(A);
			float minTri = std::min(v0, std::min(v1, v2));
			float maxTri = std::max(v0, std::max(v1, v2));

			/* 相対速度を投影 */
			float vRel = V.Dot(A);

			if (fabsf(vRel) < 1e-8f) {
				/* 平行移動しない軸：現時点で分離なら衝突なし */
				if (maxBox0 < minTri || minBox0 > maxTri) return false;
				/* 重なっているので t=0~1 全域で交差、次の軸へ */
				continue;
			}

			/* 入・出 時間を求める（符号で場合分け） */
			float t0 = (minTri - maxBox0) / vRel; // ボックスが入る時刻
			float t1 = (maxTri - minBox0) / vRel; // ボックスが出る時刻
			if (t0 > t1) std::swap(t0, t1);

			/* 時間窓を更新 */
			if (t0 > tEnter) {
				tEnter  = t0;
				bestNrm = A * (vRel > 0 ? -1.f : 1.f);
			}
			if (t1 < tExit) { tExit = t1; }

			/* 窓が壊れたら分離決定 */
			if (tEnter > tExit) return false;
			if (tExit < 0.0f) return false;  // 全体が負
			if (tEnter > 1.0f) return false; // 全体が 1 より後
		}

		/* ---------- 4. 成功 ---------- */
		outTOI = std::max(0.0f, tEnter);
		outNrm = bestNrm.Normalized();
		return true;
	}
}
