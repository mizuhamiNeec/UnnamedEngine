#include <engine/unnamed/physics/CollisionDetection.h>

#include <algorithm>
#include <cmath>

#include "engine/unnamed/primitive/Primitives.h"

namespace {
	constexpr float kContactEpsilon  = 1e-6f;  // 接触判定の許容誤差
	constexpr float kDistanceEpsilon = 1e-5f;  // 距離計算の許容誤差
	constexpr float kEdgeEpsilon     = 1e-6f;  // 辺の判定の許容誤差
	constexpr float kEpsilon         = 1e-6f;  // 汎用の許容誤差
	constexpr float kNormalEpsilon   = 1e-12f; // 法線ベクトルの正規化の許容誤差
	constexpr float kParallelEpsilon = 1e-8f;  // 平行判定の許容誤差
	constexpr float kRootEpsilon     = 1e-6f;  // ルート計算の許容誤差

	bool IsPointInsideTrianglePlane(
		const Vec3&              pointOnPlane,
		const Unnamed::Triangle& tri,
		const Vec3&              triNormal
	) {
		const Vec3 c0 = (tri.v1 - tri.v0).Cross(pointOnPlane - tri.v0);
		const Vec3 c1 = (tri.v2 - tri.v1).Cross(pointOnPlane - tri.v1);
		const Vec3 c2 = (tri.v0 - tri.v2).Cross(pointOnPlane - tri.v2);

		return triNormal.Dot(c0) >= -kEdgeEpsilon &&
		       triNormal.Dot(c1) >= -kEdgeEpsilon &&
		       triNormal.Dot(c2) >= -kEdgeEpsilon;
	}
}

namespace Unnamed::Physics {
	Vec3 ClosestPointOnTriangle(const Triangle& tri, const Vec3& point) {
		const Vec3 a = tri.v0;
		const Vec3 b = tri.v1;
		const Vec3 c = tri.v2;

		const Vec3  ab = b - a;
		const Vec3  ac = c - a;
		const Vec3  ap = point - a;
		const float d1 = ab.Dot(ap);
		const float d2 = ac.Dot(ap);
		if (d1 <= 0.0f && d2 <= 0.0f) {
			return a;
		}

		const Vec3  bp = point - b;
		const float d3 = ab.Dot(bp);
		const float d4 = ac.Dot(bp);
		if (d3 >= 0.0f && d4 <= d3) {
			return b;
		}

		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
			const float v = d1 / (d1 - d3);
			return a + ab * v;
		}

		const Vec3  cp = point - c;
		const float d5 = ab.Dot(cp);
		const float d6 = ac.Dot(cp);
		if (d6 >= 0.0f && d5 <= d6) {
			return c;
		}

		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
			const float w = d2 / (d2 - d6);
			return a + ac * w;
		}

		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) {
			const Vec3  bc      = c - b;
			const float bcLenSq = bc.Dot(bc);
			if (bcLenSq <= 1e-12f) {
				return b;
			}
			const float t =
				std::clamp((point - b).Dot(bc) / bcLenSq, 0.0f, 1.0f);
			return b + bc * t;
		}

		const Vec3  n      = ab.Cross(ac);
		const float nLenSq = n.SqrLength();
		if (nLenSq <= 1e-12f) {
			return a;
		}
		const Vec3  nn   = n / std::sqrt(nLenSq);
		const float dist = (point - a).Dot(nn);
		return point - nn * dist;
	}

	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	) {
		float tMin = ray.tMin;
		tMaxOut    = ray.tMax;

		for (int i = 0; i < 3; ++i) {
			if (fabs(ray.dir[i]) < 1e-8f) {
				if (
					ray.origin[i] < aabb.min[i] || ray.origin[i] > aabb.max[i]
				) {
					return false;
				}
			} else {
				const float invD = 1.0f / ray.dir[i];
				float       t1   = (aabb.min[i] - ray.origin[i]) * invD;
				float       t2   = (aabb.max[i] - ray.origin[i]) * invD;
				if (t1 > t2) {
					std::swap(t1, t2);
				}
				tMin    = t1 > tMin ? t1 : tMin;
				tMaxOut = t2 < tMaxOut ? t2 : tMaxOut;
				if (tMin > tMaxOut) {
					return false;
				}
			}
		}
		return true;
	}

	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	) {
		const Vec3  e1  = triangle.v1 - triangle.v0;
		const Vec3  e2  = triangle.v2 - triangle.v0;
		const Vec3  p   = ray.dir.Cross(e2);
		const float det = e1.Dot(p);

		if (fabs(det) < kEpsilon) {
			return false;
		}
		const float invDet = 1.0f / det;

		const Vec3  s = ray.origin - triangle.v0;
		const float u = s.Dot(p) * invDet;
		if (u < 0.0f || u > 1.0f) {
			return false;
		}

		const Vec3  q = s.Cross(e1);
		const float v = ray.dir.Dot(q) * invDet;
		if (v < 0.0f || u + v > 1.0f) {
			return false;
		}

		const float t = e2.Dot(q) * invDet;
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
		const Vec3 H  = box0.halfSize;
		const Vec3 V  = delta; // 世界ベロシティ

		/* ---------- 2. 13 軸を列挙 ---------- */
		Vec3 axes[13];
		int  axisCount = 0;

		/* 2-1 三角形法線 */
		Vec3 triFaceNrm = (tri.v1 - tri.v0).Cross(tri.v2 - tri.v0);
		if (triFaceNrm.SqrLength() > kNormalEpsilon) {
			triFaceNrm.Normalize();
		} else {
			triFaceNrm = Vec3::zero;
		}
		axes[axisCount++] = triFaceNrm;

		/* 2-2 ボックス軸 (世界 XYZ) */
		axes[axisCount++] = {1, 0, 0};
		axes[axisCount++] = {0, 1, 0};
		axes[axisCount++] = {0, 0, 1};

		/* 2-3 triEdge × boxAxis (交叉積) */
		const Vec3 e0          = tri.v1 - tri.v0;
		const Vec3 e1          = tri.v2 - tri.v1;
		const Vec3 e2          = tri.v0 - tri.v2;
		Vec3       triEdges[3] = {e0, e1, e2};
		for (Vec3 e : triEdges) {
			if (e.Dot(e) < 1e-10f) {
				continue; // degenerate
			}
			Vec3 n0 = e.Cross(Vec3::right).Normalized();
			Vec3 n1 = e.Cross(Vec3::up).Normalized();
			Vec3 n2 = e.Cross(Vec3::forward).Normalized();
			if (n0.Dot(n0) > 1e-6f) {
				axes[axisCount++] = n0;
			}
			if (n1.Dot(n1) > 1e-6f) {
				axes[axisCount++] = n1;
			}
			if (n2.Dot(n2) > 1e-6f) {
				axes[axisCount++] = n2;
			}
		}

		float tEnter = 0.0f; // global [0,1]
		float tExit  = 1.0f;
		Vec3  bestNrm{0, 0, 0};

		/* ---------- 3. 各軸で連続衝突テスト ---------- */
		for (int a = 0; a < axisCount; ++a) {
			Vec3 A = axes[a];
			if (A.Dot(A) < 1e-8f) {
				continue; // 0 ベクトルはスキップ
			}

			/* ボックス：中心投影 ± radius */
			const float projC0 = C0.Dot(A);
			const float rBox = fabsf(A.x) * H.x + fabsf(A.y) * H.y + fabsf(A.z)
			                   * H.z;
			const float minBox0 = projC0 - rBox;
			const float maxBox0 = projC0 + rBox;

			/* 三角形：3頂点投影 min/max */
			float       v0     = tri.v0.Dot(A);
			float       v1     = tri.v1.Dot(A);
			float       v2     = tri.v2.Dot(A);
			const float minTri = std::min(v0, std::min(v1, v2));
			const float maxTri = std::max(v0, std::max(v1, v2));

			/* 相対速度を投影 */
			const float vRel = V.Dot(A);

			if (fabsf(vRel) < 1e-8f) {
				/* 平行移動しない軸：
				 * 接触のみ(深さ0付近)は分離として扱い、t=0スタックを抑制する。 */
				if (
					maxBox0 <= minTri + kContactEpsilon ||
					minBox0 >= maxTri - kContactEpsilon
				) {
					return false;
				}
				/* 重なっているので t=0~1 全域で交差、次の軸へ */
				continue;
			}

			/* 入・出 時間を求める（符号で場合分け） */
			float t0 = (minTri - maxBox0) / vRel; // ボックスが入る時刻
			float t1 = (maxTri - minBox0) / vRel; // ボックスが出る時刻
			if (t0 > t1) {
				std::swap(t0, t1);
			}

			/* 時間窓を更新 */
			if (t0 > tEnter) {
				tEnter  = t0;
				bestNrm = A * (vRel > 0 ? -1.f : 1.f);
			}
			tExit = std::min(t1, tExit);

			/* 窓が壊れたら分離決定 */
			if (tEnter > tExit) {
				return false;
			}
			if (tExit < 0.0f) {
				return false; // 全体が負
			}
			if (tEnter > 1.0f) {
				return false; // 全体が 1 より後
			}
		}

		/* ---------- 4. 成功 ---------- */
		outTOI = std::max(0.0f, tEnter);
		if (bestNrm.SqrLength() <= kNormalEpsilon) {
			if (triFaceNrm.SqrLength() > kNormalEpsilon) {
				bestNrm = triFaceNrm;
				if (bestNrm.Dot(V) > 0.0f) {
					bestNrm = -bestNrm;
				}
			} else if (V.SqrLength() > kNormalEpsilon) {
				bestNrm = -V.Normalized();
			} else {
				bestNrm = Vec3::up;
			}
		}
		outNrm = bestNrm.Normalized();
		return true;
	}

	bool SweptSphereVsTriSAT(
		const Vec3&     center,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		if (radius <= 0.0f) {
			return false;
		}

		const Vec3 p0 = center;
		const Vec3 v  = delta;

		const Vec3  e0        = tri.v1 - tri.v0;
		const Vec3  e1        = tri.v2 - tri.v0;
		Vec3        triNormal = e0.Cross(e1);
		const float triNLenSq = triNormal.SqrLength();
		if (triNLenSq > kNormalEpsilon) {
			triNormal /= std::sqrt(triNLenSq);
		} else {
			triNormal = Vec3::zero;
		}

		float candidateTimes[24];
		int   candidateCount    = 0;
		auto  pushCandidateTime = [&](const float t) {
			if (t < -kRootEpsilon || t > 1.0f + kRootEpsilon) {
				return;
			}
			const float clampedT = std::clamp(t, 0.0f, 1.0f);
			for (int i = 0; i < candidateCount; ++i) {
				if (fabsf(candidateTimes[i] - clampedT) <= 1e-5f) {
					return;
				}
			}
			if (candidateCount < 24) {
				candidateTimes[candidateCount++] = clampedT;
			}
		};

		auto addQuadraticRoots = [&](
			const float a, const float b, const float c
		) {
			if (fabsf(a) <= kParallelEpsilon) {
				if (fabsf(b) <= kParallelEpsilon) {
					return;
				}
				pushCandidateTime(-c / b);
				return;
			}

			float discriminant = b * b - 4.0f * a * c;
			if (discriminant < -kParallelEpsilon) {
				return;
			}
			discriminant         = std::max(0.0f, discriminant);
			const float sqrtDisc = std::sqrt(discriminant);
			const float inv2A    = 0.5f / a;
			pushCandidateTime((-b - sqrtDisc) * inv2A);
			pushCandidateTime((-b + sqrtDisc) * inv2A);
		};

		pushCandidateTime(0.0f);

		const float motionLenSq = v.SqrLength();

		// 面接触候補（平面との距離が半径になる時刻）
		if (triNormal.SqrLength() > kNormalEpsilon) {
			const float planeDist0 = (p0 - tri.v0).Dot(triNormal);
			const float planeSpeed = v.Dot(triNormal);
			if (fabsf(planeSpeed) > kParallelEpsilon) {
				const float targetDists[2] = {radius, -radius};
				for (float targetDist : targetDists) {
					const float t = (targetDist - planeDist0) / planeSpeed;
					if (t < -kRootEpsilon || t > 1.0f + kRootEpsilon) {
						continue;
					}
					const float clampedT = std::clamp(t, 0.0f, 1.0f);
					const Vec3  cAtT     = p0 + v * clampedT;
					const Vec3  pOnPlane = cAtT - triNormal * targetDist;
					if (IsPointInsideTrianglePlane(pOnPlane, tri, triNormal)) {
						pushCandidateTime(clampedT);
					}
				}
			}
		}

		// 辺接触候補（移動点と三角形辺の最短距離が半径になる時刻）
		const Vec3 edgeStarts[3] = {tri.v0, tri.v1, tri.v2};
		const Vec3 edgeEnds[3]   = {tri.v1, tri.v2, tri.v0};
		for (int i = 0; i < 3; ++i) {
			const Vec3  edge    = edgeEnds[i] - edgeStarts[i];
			const float edgeLen = edge.Dot(edge);
			if (edgeLen <= kNormalEpsilon) {
				continue;
			}

			const Vec3  m          = p0 - edgeStarts[i];
			const float mdotEdge   = m.Dot(edge);
			const float vdotEdge   = v.Dot(edge);
			const float invEdgeLen = 1.0f / edgeLen;

			const float s0 = mdotEdge * invEdgeLen;
			const float sd = vdotEdge * invEdgeLen;
			const Vec3  w0 = m - edge * s0;
			const Vec3  wd = v - edge * sd;

			const float a = wd.Dot(wd);
			const float b = 2.0f * w0.Dot(wd);
			const float c = w0.Dot(w0) - radius * radius;

			float roots[2] = {};
			int   rootCnt  = 0;
			if (fabsf(a) <= kParallelEpsilon) {
				if (fabsf(b) > kParallelEpsilon) {
					roots[rootCnt++] = -c / b;
				}
			} else {
				float discriminant = b * b - 4.0f * a * c;
				if (discriminant >= -kParallelEpsilon) {
					discriminant      = std::max(0.0f, discriminant);
					const float sqrtD = std::sqrt(discriminant);
					const float inv2A = 0.5f / a;
					roots[rootCnt++]  = (-b - sqrtD) * inv2A;
					roots[rootCnt++]  = (-b + sqrtD) * inv2A;
				}
			}

			for (int r = 0; r < rootCnt; ++r) {
				const float t = roots[r];
				if (t < -kRootEpsilon || t > 1.0f + kRootEpsilon) {
					continue;
				}
				const float s = (mdotEdge + vdotEdge * t) * invEdgeLen;
				if (s >= -kRootEpsilon && s <= 1.0f + kRootEpsilon) {
					pushCandidateTime(t);
				}
			}
		}

		// 頂点接触候補（移動点と頂点距離が半径になる時刻）
		const Vec3 vertices[3] = {tri.v0, tri.v1, tri.v2};
		for (const Vec3& vertex : vertices) {
			const Vec3  toVertex = p0 - vertex;
			const float a        = motionLenSq;
			const float b        = 2.0f * toVertex.Dot(v);
			const float c        = toVertex.Dot(toVertex) - radius * radius;
			addQuadraticRoots(a, b, c);
		}

		float       bestTOI   = FLT_MAX;
		Vec3        bestNrm   = Vec3::zero;
		bool        hasHit    = false;
		const float maxDist   = radius + kDistanceEpsilon;
		const float maxDistSq = maxDist * maxDist;

		auto evaluateCandidate = [&](
			const float t, const Vec3& fallbackNormal
		) {
			const Vec3  centerAtT = p0 + v * t;
			const Vec3  closest   = ClosestPointOnTriangle(tri, centerAtT);
			const Vec3  sep       = centerAtT - closest;
			const float distSq    = sep.SqrLength();
			if (distSq > maxDistSq) {
				return;
			}

			Vec3 normal = sep;
			if (distSq > kNormalEpsilon) {
				normal /= std::sqrt(distSq);
			} else {
				normal                    = fallbackNormal;
				const float fallbackLenSq = normal.SqrLength();
				if (fallbackLenSq > kNormalEpsilon) {
					normal /= std::sqrt(fallbackLenSq);
				} else if (triNormal.SqrLength() > kNormalEpsilon) {
					normal = triNormal;
				} else if (motionLenSq > kNormalEpsilon) {
					normal = -v.Normalized();
				} else {
					normal = Vec3::up;
				}
			}

			if (motionLenSq > kNormalEpsilon && normal.Dot(v) > 0.0f) {
				normal = -normal;
			}

			if (!hasHit || t < bestTOI) {
				hasHit  = true;
				bestTOI = t;
				bestNrm = normal;
			}
		};

		const Vec3 fallbackNormal =
			triNormal.SqrLength() > kNormalEpsilon ? triNormal : Vec3::up;
		for (int i = 0; i < candidateCount; ++i) {
			evaluateCandidate(candidateTimes[i], fallbackNormal);
		}

		if (!hasHit) {
			return false;
		}

		outTOI    = std::clamp(bestTOI, 0.0f, 1.0f);
		outNormal = bestNrm.Normalized();
		return true;
	}

	bool SweptCylinderVsTriSAT(
		const Vec3&     base,
		float           halfHeight,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		base, halfHeight, radius, delta, tri, outTOI, outNormal;
		return false;
	}

	bool SweptCapsuleVsTriSAT(
		const Vec3&     a,
		const Vec3&     b,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		a, b, radius, delta, tri, outTOI, outNormal;
		return false;
	}

	bool BoxVsTriangleOverlap(
		const Box&      box,
		const Triangle& tri,
		Vec3&           outNormal,
		float&          outDepth
	) {
		// 1) テストする軸 13 本
		Vec3       axes[13];
		int        axisCnt    = 0;
		const Vec3 boxAxes[3] = {Vec3::right, Vec3::up, Vec3::forward};

		// (a) ボックスのローカル軸
		axes[axisCnt++] = boxAxes[0];
		axes[axisCnt++] = boxAxes[1];
		axes[axisCnt++] = boxAxes[2];

		// (b) 三角形の面法線
		Vec3 triN = (tri.v1 - tri.v0).Cross(tri.v2 - tri.v0);
		if (!triN.IsZero()) {
			triN.Normalize();
			axes[axisCnt++] = triN;
		}

		// (c) エッジ × ボックス軸
		Vec3 triEdges[3] = {tri.v1 - tri.v0, tri.v2 - tri.v1, tri.v0 - tri.v2};
		for (auto triEdge : triEdges) {
			for (const Vec3& boxAxis : boxAxes) {
				// SAT の 9 本は edge × boxAxis で固定し、triNormal との交差軸を混ぜない。
				Vec3 axis = triEdge.Cross(boxAxis);
				if (!axis.IsZero()) {
					axis.Normalize();
					axes[axisCnt++] = axis;
				}
			}
		}

		// 2) 各軸で投影レンジを比べ、最小オーバラップを記録
		float bestDepth = FLT_MAX;
		Vec3  bestAxis  = Vec3::zero;

		for (int i = 0; i < axisCnt; ++i) {
			const Vec3& ax = axes[i];

			// Box（中心 + half）の投影
			const float boxCenter = ax.Dot(box.center);
			const float boxExtent = std::abs(ax.x) * box.halfSize.x +
			                        std::abs(ax.y) * box.halfSize.y +
			                        std::abs(ax.z) * box.halfSize.z;
			float boxMin = boxCenter - boxExtent;
			float boxMax = boxCenter + boxExtent;

			// Triangle
			float triMin = ax.Dot(tri.v0);
			float triMax = triMin;
			float d1     = ax.Dot(tri.v1);
			float d2     = ax.Dot(tri.v2);
			triMin       = std::min({triMin, d1, d2});
			triMax       = std::max({triMax, d1, d2});

			// オーバラップ量（符号付き）
			const float overlap = std::min(boxMax, triMax) - std::max(
				                      boxMin, triMin
			                      );
			if (overlap <= kContactEpsilon) {
				return false; // 分離軸発見 → 衝突無し
			}

			if (overlap < bestDepth) {
				bestDepth = overlap;
				bestAxis  = ax;
			}
		}

		// 3) bestAxis 方向へ押し出し
		// 軸種別（面/辺）に依存しないよう、三角形重心を基準に符号を決めます。
		const Vec3 triCentroid = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
		outNormal = (box.center - triCentroid).Dot(bestAxis) >= 0.0f ?
			            bestAxis :
			            -bestAxis;
		outDepth = bestDepth;
		return true;
	}
}
