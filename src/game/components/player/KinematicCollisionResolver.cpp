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

	bool isGrounded = GroundCheck(position);

	mScene->SetWorldPos(position);
	mData->velocity   = velocity;
	mData->isGrounded = isGrounded;

	UpdateEntityHullDimensions();
	ResolvePenetration();

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

	constexpr int kMaxIterations = 4;
	for (int iter = 0; iter < kMaxIterations; ++iter) {
		UPhysics::Hit hit{};

		if (!mPhysicsEngine->BoxOverlap(*mHull, &hit)) {
			break;
		}

		if (!hit.hitEntity) break;

		auto* otherCollider = hit.hitEntity->GetComponent<AABBCollider>();
		if (!otherCollider) break;

		Vec3 otherMin, otherMax;
		{
			auto [localMin, localMax] = otherCollider->AABB();
			Vec3 offset   = otherCollider->Offset();
			Vec3 otherPos = hit.hitEntity->GetTransform()->GetWorldPos();
			otherMin      = otherPos + offset + localMin;
			otherMax      = otherPos + offset + localMax;
		}

		Vec3 myMin = mHull->center - mHull->halfSize;
		Vec3 myMax = mHull->center + mHull->halfSize;

		float overlapX = std::min(myMax.x, otherMax.x) - std::max(myMin.x, otherMin.x);
		float overlapY = std::min(myMax.y, otherMax.y) - std::max(myMin.y, otherMin.y);
		float overlapZ = std::min(myMax.z, otherMax.z) - std::max(myMin.z, otherMin.z);

		if (overlapX <= 0 || overlapY <= 0 || overlapZ <= 0) break;

		float minOverlap = overlapX;
		auto  pushDir    = Vec3(1, 0, 0);

		if (overlapY < minOverlap) {
			minOverlap = overlapY;
			pushDir    = Vec3(0, 1, 0);
		}
		if (overlapZ < minOverlap) {
			minOverlap = overlapZ;
			pushDir    = Vec3(0, 0, 1);
		}

		Vec3 otherCenter = (otherMin + otherMax) * 0.5f;
		Vec3 myCenter    = mHull->center;

		float dirCheck = (myCenter - otherCenter).Dot(pushDir);
		if (dirCheck < 0) { pushDir = -pushDir; }

		Vec3 separation = pushDir * (minOverlap + 0.001f);

		mScene->SetWorldPos(mScene->GetWorldPos() + separation);
		UpdateEntityHullDimensions();

		float velProjected = mData->velocity.Dot(pushDir);
		if (velProjected < 0) { mData->velocity -= pushDir * velProjected; }
	}
}

int KinematicCollisionResolver::SlideMove(Vec3& position, Vec3& velocity, const float timeTotal) {
	float timeLeft = std::max(0.0f, timeTotal);

	std::array<Vec3, kMaxClipPlanes> planes{};
	int                              numplanes = 0;

	for (int bumpcount = 0; bumpcount < kMaxBumps && timeLeft > 0.0f; ++bumpcount) {
		Unnamed::Box box = BuildHullAtFeet(position);

		Vec3  move    = velocity * timeLeft;
		float moveLen = move.Length();
		if (moveLen <= 1e-7f) break;

		Vec3  dir     = move / moveLen;
		float castLen = moveLen + CastSkinM();

		UPhysics::Hit hit{};
		if (!mPhysicsEngine->BoxCast(box, dir, castLen, &hit)) {
			position += move;
			break;
		}

		const float travel  = std::clamp(hit.t, 0.0f, castLen);
		const float allowed = std::min(moveLen, std::max(0.0f, travel - SkinM()));
		float       usedFrac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;
		usedFrac             = std::clamp(usedFrac, kFracEps, 1.0f);

		position += dir * allowed;
		timeLeft *= (1.0f - usedFrac);

		Vec3 normal = hit.normal;
		if (!normal.IsZero()) normal.Normalize();

		int i;
		for (i = 0; i < numplanes; i++) {
			if (planes[i].Dot(normal) > 0.99f) {
				velocity += normal;
				break;
			}
		}
		if (i < numplanes) continue;

		if (numplanes < kMaxClipPlanes) { planes[numplanes++] = normal; }

		if (numplanes == 1) {
			velocity = ClipVelocity(velocity, planes[0], 1.0f);
		} else {
			int j;
			for (j = 0; j < numplanes; j++) {
				velocity = ClipVelocity(velocity, planes[j], 1.0f);

				int k;
				for (k = 0; k < numplanes; k++) {
					if (k == j) continue;
					if (velocity.Dot(planes[k]) < 0) break;
				}
				if (k == numplanes) break;
			}

			if (j == numplanes) {
				if (numplanes != 2) {
					velocity = Vec3::zero;
					break;
				}
				Vec3 planeDir = planes[0].Cross(planes[1]);
				planeDir.Normalize();
				velocity = planeDir * velocity.Dot(planeDir);
			}
		}
	}

	return numplanes;
}

void KinematicCollisionResolver::StepMove(Vec3& position, Vec3& velocity, const float timeTotal) {
	const Vec3 startPos = position;
	const Vec3 startVel = velocity;

	SlideMove(position, velocity, timeTotal);

	Vec3  down    = position;
	float downVel = velocity.y;

	position = startPos;
	velocity = startVel;

	position += Vec3::up * StepHeightM();

	Unnamed::Box  boxUp = BuildHullAtFeet(position);
	UPhysics::Hit ov{};
	if (mPhysicsEngine->BoxOverlap(boxUp, &ov)) {
		position   = down;
		velocity.y = downVel;
		return;
	}

	SlideMove(position, velocity, timeTotal);

	Unnamed::Box  boxAt = BuildHullAtFeet(position);
	UPhysics::Hit downHit{};
	if (mPhysicsEngine->BoxCast(
		boxAt, -Vec3::up,
		StepHeightM() + RestOffsetM(), &downHit
	)) {
		const float threshold = mData->groundNormalY;
		if (downHit.normal.y >= threshold) {
			float drop = std::max(0.0f, downHit.t - RestOffsetM());
			position   += -Vec3::up * drop;
		}
	}

	const float downDist = (Vec3(down.x - startPos.x,     0.0f, down.z - startPos.z)).Length();
	const float upDist   = (Vec3(position.x - startPos.x, 0.0f, position.z - startPos.z)).Length();

	if (downDist >= upDist) {
		position   = down;
		velocity.y = downVel;
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
	const float backoff = vel.Dot(normal) * overbounce;
	Vec3        out     = vel - normal * backoff;
	if (std::fabs(out.x) < 1e-7f) out.x = 0.0f;
	if (std::fabs(out.y) < 1e-7f) out.y = 0.0f;
	if (std::fabs(out.z) < 1e-7f) out.z = 0.0f;
	return out;
}
