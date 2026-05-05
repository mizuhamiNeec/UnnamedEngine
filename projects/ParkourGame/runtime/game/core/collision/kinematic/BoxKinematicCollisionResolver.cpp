#include "BoxKinematicCollisionResolver.h"

#include <algorithm>
#include <array>

#include "core/math/Math.h"
#include "engine/physics/core/Physics.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		constexpr float    kEpsilon = 1e-6f; // 小さい値。
		constexpr float    kNormalEpsilon = 1e-12f; // 法線ベクトルの長さがこれ以下なら無効とみなす。
		constexpr float    kDuplicatePlaneDot = 0.97f; // 同一平面とみなす法線の閾値
		constexpr float    kSkinHu = 1.0f; // 衝突面からの停止距離（HU）
		constexpr float    kOverbounce = 1.01f;
		constexpr uint32_t kMaxClipPlanes = 5;
		constexpr uint32_t kMaxSlideBump = 8;
		constexpr float    kMinConsumedFrac = 1e-5f;
		constexpr float    kZeroToiEpsilon = 1e-6f;
		constexpr float    kZeroToiConsumed = 0.05f;
		constexpr float    kZeroToiNonBlockingDot = 1e-4f;
		constexpr float    kTouchDepthRatio = 0.01f;
		constexpr float    kGroundNormalY = 0.7f; // ステップダウン判定の法線の最小Y成分
		constexpr float    kStepSelectHorizEps = 1e-8f;
		constexpr float    kStepSelectDownEps = 1e-5f;
		constexpr uint32_t kMaxDepenetrationIters = 6;
		constexpr int      kDepenetrationHitBuffer = 64;
		constexpr float    kDepenetrationDepthEpsilon = 1e-5f;
		constexpr float    kDepenetrationMaxPushHu = 6.0f;
		constexpr float    kProbeRadiusScale = 1.0f;

		[[nodiscard]] float HitDistanceM(
			const Physics::Hit& hit,
			const float         castLength
		) {
			return std::clamp(hit.toi, 0.0f, 1.0f) * std::max(0.0f, castLength);
		}
	}

	void BoxKinematicCollisionResolver::UpdateHull(
		const Vec3& pos, const Vec3& halfExtents
	) {
		mHull = {
			.center   = pos,
			.halfSize = halfExtents
		};
	}

	void BoxKinematicCollisionResolver::SlideMove(
		Vec3& position, Vec3& velocity, const float timeTotal
	) const {
		mCollisionDebugState                = {};
		mCollisionDebugState.startPosition  = position;
		mCollisionDebugState.inputVelocity  = velocity;
		const auto FinalizeDebugState = [this, &position, &velocity]() {
			mCollisionDebugState.endPosition  = position;
			mCollisionDebugState.outputVelocity = velocity;
		};

		// 移動開始時の開始重なりを先に解消します。
		if (!RecoverFromPenetration(
			position, -velocity, RECOVER_REASON::SLIDE_START
		)) {
			velocity = Vec3::zero;
			FinalizeDebugState();
			return;
		}

		int         numPlanes = 0;
		float       timeLeft  = timeTotal;
		const float skin      = SkinM();

		const Vec3 primalVelocity = velocity;

		std::array<Vec3, kMaxClipPlanes> planes{};

		for (uint32_t bumpCount = 0; bumpCount < kMaxSlideBump; ++bumpCount) {
			if (velocity.SqrLength() < 1e-12f) {
				break;
			}
			const float preClipSpeed = velocity.Length();

			Vec3  move    = velocity * timeLeft;
			float moveLen = move.Length();
			if (moveLen < 1e-7f) {
				break;
			}

			Vec3 dir = move / moveLen;

			Box box    = mHull;
			box.center = position;
			Physics::Hit hit{};

			const float castLength = moveLen + skin;

			if (!mEngine->BoxCast(
				box, dir, castLength, &hit
			)) {
				// sweep で見落とした終点めり込みを防ぐため、終点の重なりを再検査
				const Vec3 targetPos = position + move;
				if (IsHullOverlapping(targetPos, nullptr)) {
					Vec3 recoveredPos = targetPos;
					if (!RecoverFromPenetration(
						recoveredPos, -dir, RECOVER_REASON::END_POSITION_OVERLAP
					)) {
						velocity = Vec3::zero;
						break;
					}
					position = recoveredPos;
				} else {
					position = targetPos;
				}
				break;
			}

			if (hit.allsolid) {
				RecordBlockingHit(box.center, dir, castLength, hit);
				if (RecoverFromPenetration(
					position, -velocity, RECOVER_REASON::HIT_ALL_SOLID
				)) {
					numPlanes = 0;
					continue;
				}
				velocity = Vec3::zero;
				FinalizeDebugState();
				return;
			}

			// ─── 前進距離を計算 ───
			// hit.toi = [0, 1] の TOI。実距離に変換してから面から SkinM() 手前で止まる。moveLen を上限に。
			const float hitDistance =
				HitDistanceM(hit, castLength);
			float allowedDist = std::min(
				moveLen,
				std::max(0.0f, hitDistance - skin)
			);

			// 法線を正規化
			Vec3 normal = hit.normal;
			if (normal.SqrLength() > kNormalEpsilon) {
				normal.Normalize();
			} else {
				velocity = Vec3::zero;
				break;
			}

			const bool shallowStartSolid = hit.startSolid &&
			                               hit.depth <= skin * kTouchDepthRatio;
			const bool zeroToiContact =
				(!hit.startSolid || shallowStartSolid) &&
				hitDistance <= kZeroToiEpsilon &&
				allowedDist <= kEpsilon;
			const float moveIntoPlane = dir.Dot(normal);
			if (
				zeroToiContact &&
				moveIntoPlane > -kZeroToiNonBlockingDot
			) {
				// 非侵入のゼロTOI接触（床なめ等）は進行阻害として扱わず、
				// 少量前進して次のキャストで実ブロッキング面を探します。
				const float touchAdvance = std::min(moveLen, skin);
				if (touchAdvance > 1e-7f) {
					position += dir * touchAdvance;
				}
				float consumedFrac = std::clamp(
					touchAdvance / moveLen, 0.0f, 1.0f
				);
				consumedFrac = std::max(consumedFrac, kZeroToiConsumed);
				timeLeft *= (1.0f - consumedFrac);
				if (timeLeft < 1e-7f) {
					break;
				}
				continue;
			}

			RecordBlockingHit(box.center, dir, castLength, hit);

			if (hit.startSolid && !shallowStartSolid) {
				if (!RecoverFromPenetration(
					position, -normal, RECOVER_REASON::HIT_START_SOLID
				)) {
					velocity = Vec3::zero;
					FinalizeDebugState();
					return;
				}
				numPlanes = 0;
				continue;
			}
			if (allowedDist > 1e-7f) {
				position += dir * allowedDist;
				// 前進に成功したら面リストをリセット（新しい位置で再出発）
				numPlanes = 0;
			}

			// 残り時間は実移動量基準で消費し、停滞回避のため最小消費のみ保証する。
			float consumedFrac = std::clamp(allowedDist / moveLen, 0.0f, 1.0f);
			consumedFrac       = std::max(
				consumedFrac,
				zeroToiContact ? kZeroToiConsumed : kMinConsumedFrac
			);
			timeLeft *= (1.0f - consumedFrac);
			if (timeLeft < 1e-7f) {
				break;
			}

			// ─── 同一面チェック ───
			bool duplicatePlane = false;
			for (int i = 0; i < numPlanes; ++i) {
				if (planes[i].Dot(normal) > kDuplicatePlaneDot) {
					// 同じ面にもう一度当たった。速度を面からクリップしてやり直す。
					velocity = ClipVelocity(velocity, normal, kOverbounce);
					const float clippedSpeed = velocity.Length();
					if (clippedSpeed > preClipSpeed + kEpsilon && clippedSpeed >
					    kEpsilon) {
						velocity *= (preClipSpeed / clippedSpeed);
					}
					duplicatePlane = true;
					break;
				}
			}
			if (duplicatePlane) {
				continue;
			}

			if (numPlanes >= kMaxClipPlanes) {
				velocity = Vec3::zero;
				break;
			}
			planes[numPlanes++] = normal;

			// ─── クリッピング ───
			if (numPlanes == 1) {
				// 面が1枚 — 普通にクリップ
				velocity = ClipVelocity(velocity, planes[0], kOverbounce);
			} else {
				// 複数面: 今の速度をクリップし、全面と整合する解を探す
				Vec3 clipped;
				int  i;
				for (i = 0; i < numPlanes; ++i) {
					clipped = ClipVelocity(velocity, planes[i], kOverbounce);

					bool valid = true;
					for (int j = 0; j < numPlanes; ++j) {
						if (j == i) {
							continue;
						}
						if (clipped.Dot(planes[j]) < 0.0f) {
							valid = false;
							break;
						}
					}
					if (valid) {
						break;
					}
				}

				if (i == numPlanes) {
					if (numPlanes == 2) {
						// 2面の交線に沿って滑る
						Vec3  creaseDir = planes[0].Cross(planes[1]);
						float cLen      = creaseDir.Length();
						if (cLen > 1e-7f) {
							creaseDir = creaseDir / cLen;
							velocity  = creaseDir * velocity.Dot(creaseDir);
						} else {
							velocity = Vec3::zero;
							break;
						}
					} else {
						velocity = Vec3::zero;
						break;
					}
				} else {
					velocity = clipped;
				}
			}
			const float clippedSpeed = velocity.Length();
			if (clippedSpeed > preClipSpeed + kEpsilon && clippedSpeed >
			    kEpsilon) {
				velocity *= (preClipSpeed / clippedSpeed);
			}

			// dead-stop: クリップ後の速度が元の方向と逆転したら停止
			if (velocity.SqrLength() <= kEpsilon) {
				velocity = Vec3::zero;
				break;
			}
			if (velocity.Dot(primalVelocity) < 0.0f) {
				velocity = Vec3::zero;
				break;
			}
		}

		FinalizeDebugState();
	}

	void BoxKinematicCollisionResolver::StepMove(
		Vec3& position, Vec3& velocity, float stepHeight, float timeTotal
	) const {
		const float stepHeightM = std::max(0.0f, Math::HtoM(stepHeight));
		if (stepHeightM <= kEpsilon) {
			SlideMove(position, velocity, timeTotal);
			return;
		}

		auto HorizDistSq = [](const Vec3& a, const Vec3& b) -> float {
			float dx = a.x - b.x;
			float dz = a.z - b.z;
			return dx * dx + dz * dz;
		};

		const auto SnapDownWalkable = [this](
			Vec3& targetPos, const float maxDrop
		) {
			if (maxDrop <= 0.0f) {
				return false;
			}

			Box box    = mHull;
			box.center = targetPos;

			const float  skin       = SkinM();
			const float  castLength = maxDrop + skin;
			Physics::Hit hit{};
			if (!mEngine->BoxCast(box, Vec3::down, castLength, &hit)) {
				return false;
			}

			const bool walkable = hit.startSolid || hit.allsolid ||
			                      hit.normal.y > kGroundNormalY;
			if (!walkable) {
				return false;
			}

			const float hitDistance =
				std::clamp(hit.toi, 0.0f, 1.0f) * castLength;
			const float drop = std::clamp(hitDistance - skin, 0.0f, maxDrop);
			targetPos        += Vec3::down * drop;
			return true;
		};

		// 1) 通常のSlideMoveを先に実行（baseline）
		const Vec3 savedPos = position;
		const Vec3 savedVel = velocity;

		const Vec3  desiredMove        = savedVel * timeTotal;
		const float desiredHorizDistSq =
			desiredMove.x * desiredMove.x + desiredMove.z * desiredMove.z;

		SlideMove(position, velocity, timeTotal);
		Vec3 downPos = position;
		Vec3 downVel = velocity;
		if (SnapDownWalkable(downPos, stepHeightM) && downVel.y < 0.0f) {
			downVel.y = 0.0f;
		}

		const float downDistSq = HorizDistSq(downPos, savedPos);
		// baselineで十分進めているときは、不要なステップアップを試さない。
		if (downDistSq + kStepSelectHorizEps >= desiredHorizDistSq) {
			position = downPos;
			velocity = downVel;
			return;
		}

		// 2) ステップ移動候補
		position = savedPos;
		velocity = savedVel;

		float stepRise = stepHeightM;
		{
			Box          boxUp = mHull;
			Physics::Hit upHit{};
			const float  skin         = SkinM();
			const float  upCastLength = stepHeightM + skin;
			boxUp.center              = position;

			if (mEngine->BoxCast(boxUp, Vec3::up, upCastLength, &upHit)) {
				const float hitDistance =
					std::clamp(upHit.toi, 0.0f, 1.0f) *
					upCastLength;

				stepRise = std::clamp(hitDistance - skin, 0.0f, stepHeightM);

				if (stepRise < skin) {
					position = downPos;
					velocity = downVel;
					return;
				}
			}

			position += Vec3::up * stepRise;
		}

		SlideMove(position, velocity, timeTotal);

		// 上げた分 + 許容ステップダウン分だけ地面へスナップを試みる。
		const float maxDrop    = stepRise + stepHeightM;
		const bool  upGrounded = SnapDownWalkable(position, maxDrop);
		if (!upGrounded) {
			position = downPos;
			velocity = downVel;
			return;
		}

		const float upDistSq       = HorizDistSq(position, savedPos);
		const bool  climbedTooHigh = position.y >
		                            (savedPos.y + stepHeightM +
		                             kStepSelectDownEps);

		if (climbedTooHigh || upDistSq <= downDistSq + kStepSelectHorizEps) {
			position = downPos;
			velocity = downVel;
			return;
		}

		velocity.y = std::max(velocity.y, 0.0f);
	}

	bool BoxKinematicCollisionResolver::ProbeGround(
		const Vec3& position, const float maxDistance, Physics::Hit* outHit
	) const {
		if (!mEngine || maxDistance <= 0.0f) {
			return false;
		}

		Box box    = mHull;
		box.center = position;

		Physics::Hit hit{};
		const float  castDistance = maxDistance + CastSkinM();
		if (!mEngine->BoxCast(box, Vec3::down, castDistance, &hit)) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}
		return true;
	}

	const BoxKinematicCollisionResolver::CollisionDebugState&
	BoxKinematicCollisionResolver::GetCollisionDebugState() const {
		return mCollisionDebugState;
	}

	void BoxKinematicCollisionResolver::RecordBlockingHit(
		const Vec3&        castStart,
		const Vec3&        castDir,
		const float        castLength,
		const Physics::Hit& hit
	) const {
		mCollisionDebugState.hasBlockingHit    = true;
		++mCollisionDebugState.blockingHitCount;
		mCollisionDebugState.blockingCastStart = castStart;
		mCollisionDebugState.blockingCastDir   = castDir;
		mCollisionDebugState.blockingCastLength = castLength;
		mCollisionDebugState.blockingHit       = hit;
	}

	bool BoxKinematicCollisionResolver::IsHullOverlapping(
		const Vec3&   position,
		Physics::Hit* outHit
	) const {
		if (!mEngine) {
			return false;
		}

		Box box    = mHull;
		box.center = position;
		return mEngine->BoxOverlap(box, outHit);
	}

	bool BoxKinematicCollisionResolver::RecoverFromPenetration(
		Vec3& position,
		const Vec3& preferredDirection,
		const RECOVER_REASON reason
	) const {
		mCollisionDebugState.recoverAttempted = true;
		mCollisionDebugState.recoverSucceeded = false;
		mCollisionDebugState.recoverReason    = reason;
		mCollisionDebugState.recoverStart     = position;
		mCollisionDebugState.recoverEnd       = position;
		mCollisionDebugState.recoverOverlapCount = 0;

		if (!mEngine) {
			return false;
		}
		if (!IsHullOverlapping(position, nullptr)) {
			mCollisionDebugState.recoverSucceeded = true;
			mCollisionDebugState.recoverEnd       = position;
			return true;
		}

		const float skinM = SkinM();
		const float maxPushPerIter = Math::HtoM(kDepenetrationMaxPushHu);

		Vec3 preferredDir = preferredDirection;
		if (preferredDir.SqrLength() > kNormalEpsilon) {
			preferredDir.Normalize();
		} else {
			preferredDir = Vec3::zero;
		}

		for (uint32_t iter = 0; iter < kMaxDepenetrationIters; ++iter) {
			std::array<Physics::Hit, kDepenetrationHitBuffer> overlaps{};
			const int overlapCount = CollectOverlaps(
				position, mHull.halfSize, overlaps.data(), kDepenetrationHitBuffer
			);
			if (overlapCount <= 0) {
				mCollisionDebugState.recoverSucceeded = true;
				mCollisionDebugState.recoverEnd       = position;
				return true;
			}
			mCollisionDebugState.recoverOverlapCount = std::min(
				overlapCount,
				static_cast<int>(mCollisionDebugState.recoverOverlaps.size())
			);
			for (int i = 0; i < mCollisionDebugState.recoverOverlapCount; ++i) {
				mCollisionDebugState.recoverOverlaps[static_cast<size_t>(i)] =
					overlaps[static_cast<size_t>(i)];
			}

			Vec3  pushAccum      = Vec3::zero;
			Vec3  deepestNormal  = Vec3::zero;
			float deepestPushLen = 0.0f;
			for (int i = 0; i < overlapCount; ++i) {
				Vec3 normal = overlaps[i].normal;
				if (normal.SqrLength() <= kNormalEpsilon) {
					continue;
				}
				normal.Normalize();

				const float pushLen = overlaps[i].depth + skinM;
				if (pushLen <= kDepenetrationDepthEpsilon) {
					continue;
				}

				pushAccum += normal * pushLen;
				if (pushLen > deepestPushLen) {
					deepestPushLen = pushLen;
					deepestNormal  = normal;
				}
			}

			if (deepestPushLen <= kDepenetrationDepthEpsilon) {
				break;
			}

			Vec3 pushDir = pushAccum.SqrLength() > kNormalEpsilon ?
				               pushAccum.Normalized() :
				               deepestNormal;
			if (
				!preferredDir.IsZero() &&
				pushDir.Dot(preferredDir) < 0.0f
			) {
				pushDir += preferredDir * 0.5f;
				if (pushDir.SqrLength() > kNormalEpsilon) {
					pushDir.Normalize();
				}
			}

			const float pushDist = std::clamp(
				deepestPushLen, skinM, maxPushPerIter
			);
			position += pushDir * pushDist;
			if (!IsHullOverlapping(position, nullptr)) {
				mCollisionDebugState.recoverSucceeded = true;
				mCollisionDebugState.recoverEnd       = position;
				return true;
			}
		}

		const Vec3 searchOrigin = position;
		const std::array<Vec3, 16> probeDirs = {
			Vec3::right,
			Vec3::left,
			Vec3::forward,
			Vec3::backward,
			Vec3::up,
			Vec3::down,
			(Vec3::right + Vec3::forward).Normalized(),
			(Vec3::right + Vec3::backward).Normalized(),
			(Vec3::left + Vec3::forward).Normalized(),
			(Vec3::left + Vec3::backward).Normalized(),
			(Vec3::right + Vec3::up).Normalized(),
			(Vec3::left + Vec3::up).Normalized(),
			(Vec3::forward + Vec3::up).Normalized(),
			(Vec3::backward + Vec3::up).Normalized(),
			(Vec3::right + Vec3::down).Normalized(),
			(Vec3::left + Vec3::down).Normalized(),
		};
		const std::array<float, 8> probeRadiiHu = {
			0.25f * kProbeRadiusScale,
			0.5f * kProbeRadiusScale,
			1.0f * kProbeRadiusScale,
			2.0f * kProbeRadiusScale,
			4.0f * kProbeRadiusScale,
			8.0f * kProbeRadiusScale,
			16.0f * kProbeRadiusScale,
			24.0f * kProbeRadiusScale
		};

		for (const float radiusHu : probeRadiiHu) {
			const float radiusM = Math::HtoM(radiusHu);

			if (!preferredDir.IsZero()) {
				const Vec3 preferredCandidate = searchOrigin + preferredDir *
					radiusM;
				if (!IsHullOverlapping(preferredCandidate, nullptr)) {
					position = preferredCandidate;
					mCollisionDebugState.recoverSucceeded = true;
					mCollisionDebugState.recoverEnd       = position;
					return true;
				}
			}

			for (const Vec3& dir : probeDirs) {
				const Vec3 candidate = searchOrigin + dir * radiusM;
				if (!IsHullOverlapping(candidate, nullptr)) {
					position = candidate;
					mCollisionDebugState.recoverSucceeded = true;
					mCollisionDebugState.recoverEnd       = position;
					return true;
				}
			}
		}

		mCollisionDebugState.recoverSucceeded = !IsHullOverlapping(
			position, nullptr
		);
		mCollisionDebugState.recoverEnd = position;
		return mCollisionDebugState.recoverSucceeded;
	}

	int BoxKinematicCollisionResolver::CollectOverlaps(
		const Vec3&   position,
		const Vec3&   halfExtents,
		Physics::Hit* outHits,
		const int     maxHits
	) const {
		if (!mEngine || !outHits || maxHits <= 0) {
			return 0;
		}

		const Box box = {
			.center   = position,
			.halfSize = halfExtents
		};
		return mEngine->BoxOverlapRaw(box, outHits, maxHits);
	}

	float BoxKinematicCollisionResolver::CastSkinM() {
		return SkinM();
	}

	float BoxKinematicCollisionResolver::SkinM() {
		return Math::HtoM(kSkinHu);
	}
}
