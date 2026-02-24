#include "KinematicCollisionResolver.h"

#include <array>

#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Entity/Entity.h>

#include "MovementComponent.h"

KinematicCollisionResolver::KinematicCollisionResolver(
	UPhysics::Engine* physicsEngine,
	MovementData*     data,
	SceneComponent*   sceneComponent,
	Unnamed::Box*     hull
)
	: mPhysicsEngine(physicsEngine),
	  mData(data),
	  mScene(sceneComponent),
	  mHull(hull) {}

bool KinematicCollisionResolver::CollideAndSlide(
	const Vec3& startPos,
	const Vec3& displacement,
	Vec3&       outPos
) {
	outPos = startPos + displacement;
	if (!mPhysicsEngine) { return !displacement.IsZero(); }

	Vec3 position = startPos;
	Vec3 velocity = displacement;
	SlideMove(position, velocity, 1.0f);
	outPos = position;
	return (outPos - startPos).SqrLength() > 1e-8f;
}

void KinematicCollisionResolver::UpdateEntityHullDimensions() {
	// 足元原点
	*mHull = {
		.center = mScene->GetWorldPos() + Vec3::up * Math::HtoM(
			          mData->currentHeightHu * 0.5f
		          ),
		.halfSize = Math::HtoM(
			{
				mData->currentWidthHu * 0.5f,
				mData->currentHeightHu * 0.5f,
				mData->currentWidthHu * 0.5f
			}
		)
	};
}

void KinematicCollisionResolver::SyncHullFromComponent() {
	UpdateEntityHullDimensions();
}

void KinematicCollisionResolver::MoveWithCollisions(const float dt) {
	if (!mPhysicsEngine) {
		mScene->SetWorldPos(mScene->GetWorldPos() + mData->velocity * dt);
		mData->isGrounded = false;
		UpdateEntityHullDimensions();
		return;
	}

	// 移動前に既存の貫通を解消
	ResolvePenetration();

	Vec3 position = mScene->GetWorldPos();
	Vec3 velocity = mData->velocity;

	const float horizVelSqr = velocity.x * velocity.x + velocity.z * velocity.z;
	const bool  wantStep    = mData->wasGroundedLastFrame && (horizVelSqr > 1e-8f);

	if (wantStep) {
		StepMove(position, velocity, dt);
	} else {
		SlideMove(position, velocity, dt);
	}

	// 接地チェック
	bool isGrounded = GroundCheck(position);

	mScene->SetWorldPos(position);
	mData->velocity   = velocity;
	mData->isGrounded = isGrounded;

	UpdateEntityHullDimensions();

	// 移動後の貫通解消
	ResolvePenetration();

	// 接地中に下向き速度をゼロに
	if (mData->isGrounded && mData->velocity.y < 0.0f) {
		mData->velocity.y = 0.0f;
	}

	if (!mData->isWallRunning && !mData->isSliding) {
		mData->state = mData->isGrounded ?
			               MOVEMENT_STATE::GROUND :
			               MOVEMENT_STATE::AIR;
	}

	UpdateEntityHullDimensions();
}

void KinematicCollisionResolver::DetectAndResolveStuck(const float dt) {
	Vec3 currentPos = mScene->GetWorldPos();
	float distMoved = (currentPos - mData->lastPosition).Length();

	const bool hasInput = (mData->vecMoveInput.x != 0.0f || mData->vecMoveInput.y != 0.0f) || mData->wishJump;

	if (hasInput && distMoved < kStuckThreshold * dt) {
		mData->stuckTime += dt;
		if (mData->stuckTime >= kStuckTimeThreshold) {
			mData->isStuck = true;

			Vec3 escapeAttempts[] = {
				Vec3::up * kStuckEscapeForce,
				Vec3(1,  2,  0).Normalized() * kStuckEscapeForce,
				Vec3(-1, 2,  0).Normalized() * kStuckEscapeForce,
				Vec3(0,  2,  1).Normalized() * kStuckEscapeForce,
				Vec3(0,  2, -1).Normalized() * kStuckEscapeForce,
			};

			bool escaped = false;
			for (const Vec3& escapeVel : escapeAttempts) {
				Vec3 testPos = currentPos + escapeVel * dt * 2.0f;
				mScene->SetWorldPos(testPos);
				UpdateEntityHullDimensions();

				if (mPhysicsEngine) {
					UPhysics::Hit ov{};
					if (!mPhysicsEngine->BoxOverlap(*mHull, &ov)) {
						mData->velocity += escapeVel;
						escaped        = true;
						break;
					}
				}
			}

			if (!escaped) {
				mScene->SetWorldPos(currentPos);
				UpdateEntityHullDimensions();
			}
			mData->stuckTime = 0.0f;
		}
	} else {
		mData->stuckTime = std::max(0.0f, mData->stuckTime - dt * 2.0f);
		if (mData->stuckTime == 0.0f) { mData->isStuck = false; }
	}
	mData->lastPosition = mScene->GetWorldPos();
}

Unnamed::Box KinematicCollisionResolver::BuildHullAtFeet(const Vec3& feetPos) const {
	return Unnamed::Box{
		.center = feetPos + Vec3::up * Math::HtoM(mData->currentHeightHu * 0.5f),
		.halfSize = Math::HtoM(
			{
				mData->currentWidthHu * 0.5f,
				mData->currentHeightHu * 0.5f,
				mData->currentWidthHu * 0.5f,
			}
		)
	};
}

void KinematicCollisionResolver::ResolvePenetration() {
	if (!mPhysicsEngine) return;

	for (int iter = 0; iter < kMaxDepushIter; ++iter) {
		UpdateEntityHullDimensions();

		UPhysics::Hit hits[kMaxDepushHits];
		int hitCount = mPhysicsEngine->BoxOverlap(*mHull, hits, kMaxDepushHits);
		if (hitCount == 0) break;

		Vec3 totalPush = Vec3::zero;
		bool anyPush   = false;

		for (int i = 0; i < hitCount; ++i) {
			const UPhysics::Hit& h = hits[i];
			if (h.depth <= 0.0f) continue;

			Vec3 normal = h.normal;
			if (normal.SqrLength() < 1e-12f) continue;
			normal.Normalize();

			float pushDist = h.depth + SkinM();

			// 既に同じ方向に十分押し出し済みなら省略
			float projected = totalPush.Dot(normal);
			if (projected >= pushDist) continue;

			float remaining = pushDist - std::max(0.0f, projected);
			totalPush       = totalPush + normal * remaining;
			anyPush         = true;
		}

		if (!anyPush) break;

		mScene->SetWorldPos(mScene->GetWorldPos() + totalPush);
		UpdateEntityHullDimensions();

		// 速度補正: 押し出し方向に対して速度が「めり込む方向」なら除去
		Vec3 pushDir = totalPush.Normalized();
		float velIntoWall = mData->velocity.Dot(pushDir);
		if (velIntoWall < 0.0f) {
			mData->velocity = mData->velocity - pushDir * velIntoWall;
		}
	}
}

int KinematicCollisionResolver::SlideMove(Vec3& position, Vec3& velocity, const float timeTotal) {
	int   blocked   = 0;
	int   numplanes = 0;
	float timeLeft  = timeTotal;

	Vec3 primalVelocity = velocity;

	std::array<Vec3, kMaxClipPlanes> planes{};

	for (int bumpcount = 0; bumpcount < kMaxBumps; ++bumpcount) {
		if (velocity.SqrLength() < 1e-12f) break;

		Vec3  move    = velocity * timeLeft;
		float moveLen = move.Length();
		if (moveLen < 1e-7f) break;

		Vec3 dir = move / moveLen;

		Unnamed::Box  box = BuildHullAtFeet(position);
		UPhysics::Hit hit{};

		if (!mPhysicsEngine->BoxCast(box, dir, moveLen + CastSkinM(), &hit)) {
			// 衝突なし — 全距離移動して終了
			position += move;
			break;
		}

		// allsolid — 完全に固体内
		if (hit.allsolid) {
			velocity = Vec3::zero;
			return 4;
		}

		// ─── 前進距離を計算 ───
		// hit.t = 実距離(m)。面から SkinM() 手前で止まる。moveLen を上限に。
		float allowed = std::min(moveLen, std::max(0.0f, hit.t - SkinM()));

		if (allowed > 1e-7f) {
			position += dir * allowed;
			// 前進に成功したら面リストをリセット（新しい位置で再出発）
			numplanes = 0;
		}

		// startSolid で進めなかった場合、法線方向に押し出す
		if (hit.startSolid && allowed <= 1e-7f) {
			Vec3 pushNormal = hit.normal;
			if (pushNormal.SqrLength() > 1e-12f) {
				pushNormal.Normalize();
				position += pushNormal * SkinM();
			}
		}

		// 残り時間を消費
		float frac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;
		timeLeft  *= (1.0f - std::clamp(frac, 0.0f, 1.0f));
		if (timeLeft < 1e-7f) break;

		// 法線を正規化
		Vec3 normal = hit.normal;
		if (normal.SqrLength() > 1e-12f) normal.Normalize();

		// blocked フラグ
		if (normal.y > mData->groundNormalY) blocked |= 1;
		if (std::fabs(normal.y) <= mData->groundNormalY) blocked |= 2;

		// ─── 同一面チェック ───
		bool duplicatePlane = false;
		for (int i = 0; i < numplanes; ++i) {
			if (planes[i].Dot(normal) > 0.99f) {
				// 同じ面にもう一度当たった。速度を面からクリップしてやり直す。
				velocity = ClipVelocity(velocity, normal, kOverbounce);
				duplicatePlane = true;
				break;
			}
		}
		if (duplicatePlane) continue;

		if (numplanes >= kMaxClipPlanes) {
			velocity = Vec3::zero;
			break;
		}
		planes[numplanes++] = normal;

		// ─── クリッピング ───
		if (numplanes == 1) {
			// 面が1枚 — シンプルにクリップ
			velocity = ClipVelocity(velocity, planes[0], kOverbounce);
		} else {
			// 複数面: 現在のvelocityをクリップし、全面と整合する解を探す
			Vec3 clipped;
			int  i;
			for (i = 0; i < numplanes; ++i) {
				clipped = ClipVelocity(velocity, planes[i], kOverbounce);

				bool valid = true;
				for (int j = 0; j < numplanes; ++j) {
					if (j == i) continue;
					if (clipped.Dot(planes[j]) < 0.0f) {
						valid = false;
						break;
					}
				}
				if (valid) break;
			}

			if (i == numplanes) {
				if (numplanes == 2) {
					// 2面の交線に沿って滑る
					Vec3 creaseDir = planes[0].Cross(planes[1]);
					float cLen = creaseDir.Length();
					if (cLen > 1e-7f) {
						creaseDir = creaseDir / cLen;
						velocity = creaseDir * velocity.Dot(creaseDir);
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

		// dead-stop: クリップ後の速度が元の方向と逆転したら停止
		if (velocity.Dot(primalVelocity) < 0.0f) {
			velocity = Vec3::zero;
			break;
		}
	}

	return blocked;
}

void KinematicCollisionResolver::StepMove(Vec3& position, Vec3& velocity, const float timeTotal) {
	// PM_StepMove
	// 1) まず通常のSlideMoveを試す (baseline)
	const Vec3 savedPos = position;
	const Vec3 savedVel = velocity;

	SlideMove(position, velocity, timeTotal);

	const Vec3  downPos = position;
	const Vec3  downVel = velocity;

	// 2) ステップ移動を試す
	position = savedPos;
	velocity = savedVel;

	// 上方向にステップ高さだけ持ち上げる (BoxCastで天井衝突チェック)
	{
		Unnamed::Box  boxUp = BuildHullAtFeet(position);
		UPhysics::Hit upHit{};
		float         stepUp = StepHeightM();

		if (mPhysicsEngine->BoxCast(boxUp, Vec3::up, stepUp + SkinM(), &upHit)) {
			// 天井に当たった場合、上昇可能な分だけ上げる
			float allowed = std::max(0.0f, upHit.t - SkinM());
			if (allowed < SkinM()) {
				// ほぼ上に移動できない → baseline結果を使う
				position = downPos;
				velocity = downVel;
				return;
			}
			position += Vec3::up * allowed;
		} else {
			position += Vec3::up * stepUp;
		}
	}

	// 持ち上げた位置でSlideMove
	SlideMove(position, velocity, timeTotal);

	// 下方向にステップ高さ + 少し余分に降ろす (元の高さ + ステップ分)
	{
		Unnamed::Box  boxDown = BuildHullAtFeet(position);
		UPhysics::Hit downHit{};
		float         stepDown = StepHeightM() + RestOffsetM();

		if (mPhysicsEngine->BoxCast(boxDown, Vec3::down, stepDown, &downHit)) {
			// 何かに当たったら、歩行可能面かどうかに関わらず降ろす
			// （浮いたままになるのを防ぐ）
			float drop = std::max(0.0f, downHit.t - SkinM());
			position += Vec3::down * drop;
		}
	}

	// 水平距離で比較: ステップ移動のほうが遠くに行けた場合のみ採用
	auto horizDistSq = [](const Vec3& a, const Vec3& b) -> float {
		float dx = a.x - b.x;
		float dz = a.z - b.z;
		return dx * dx + dz * dz;
	};

	const float downDistSq = horizDistSq(downPos, savedPos);
	const float upDistSq   = horizDistSq(position, savedPos);

	if (downDistSq >= upDistSq) {
		// ステップ移動の利点なし → baseline結果を使う
		position = downPos;
		velocity = downVel;
	}
}

bool KinematicCollisionResolver::GroundCheck(Vec3& position) {
	if (mData->jumpSnapDisableTime > 0.0f || mData->velocity.y > 0.0f) {
		return false;
	}

	Unnamed::Box  box = BuildHullAtFeet(position);
	UPhysics::Hit gHit{};

	float snapRange;
	if (mData->wasGroundedLastFrame) {
		snapRange = RestOffsetM() + std::max(MaxAdhesionM(), StepHeightM());
	} else { snapRange = RestOffsetM() + CastSkinM(); }

	if (!mPhysicsEngine->BoxCast(box, Vec3::down, snapRange, &gHit)) {
		return false;
	}

	const float threshold = mData->groundNormalY;
	if (gHit.normal.y < threshold) { return false; }

	const float drop = std::max(0.0f, gHit.t - RestOffsetM());
	position         += -Vec3::up * drop;

	mData->lastGroundNormal = gHit.normal;
	mData->lastGroundDistM  = gHit.t;

	return true;
}

Vec3 KinematicCollisionResolver::ClipVelocity(const Vec3& vel, const Vec3& normal, float overbounce) {
	// PM_ClipVelocity
	// backoff = DotProduct(in, normal) * overbounce
	// out[i] = in[i] - normal[i] * backoff
	const float backoff = vel.Dot(normal) * overbounce;
	Vec3        out     = vel - normal * backoff;

	// 微小値をゼロに丸める (STOP_EPSILON = 0.1 HU/s 相当)
	constexpr float kStopEps = 1e-6f; // メートル単位の微小値
	if (std::fabs(out.x) < kStopEps) out.x = 0.0f;
	if (std::fabs(out.y) < kStopEps) out.y = 0.0f;
	if (std::fabs(out.z) < kStopEps) out.z = 0.0f;
	return out;
}
