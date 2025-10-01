#include <game/components/CharacterMovement.h>

#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/Entity/Entity.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/uphysics/PhysicsTypes.h>

#include <runtime/physics/core/UPhysics.h>

CharacterMovement::~CharacterMovement() {
}

void CharacterMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mScene = mOwner->GetTransform();
}

void CharacterMovement::Update(const float deltaTime) {
	mDeltaTime = deltaTime;
	mPosition  = mScene->GetLocalPos();

	Move();

	Debug::DrawArrow(mScene->GetWorldPos(), mVelocity * 0.25f,
	                 Vec4::yellow);

	const float width  = Math::HtoM(mCurrentWidthHu);
	const float height = Math::HtoM(mCurrentHeightHu);
	Debug::DrawBox(
		mScene->GetWorldPos() + (Vec3::up * height * 0.5f),
		Quaternion::Euler(Vec3::zero),
		Vec3(width, height, width),
		mIsGrounded ? Vec4::green : Vec4::red);
}

void CharacterMovement::DrawInspectorImGui() {
}

void CharacterMovement::Move() {
}

void CharacterMovement::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();
	mVelocity.y -= Math::HtoM(gravity) * 0.5f * mDeltaTime;
}

void CharacterMovement::ApplyFriction(const float frictionValue) {
	Vec3 vel = Math::MtoH(mVelocity);

	vel.y       = 0.0f;
	float speed = vel.Length();
	if (speed < 0.1f) {
		return;
	}

	float drop = 0.0f;
	if (mIsGrounded) {
		const float friction  = frictionValue;
		const float stopspeed = ConVarManager::GetConVar("sv_stopspeed")->
			GetValueAsFloat();
		const float control = speed < stopspeed ? stopspeed : speed;
		drop                = control * friction * mDeltaTime;
	}

	float newspeed = speed - drop;

	newspeed = std::max<float>(newspeed, 0);

	if (newspeed != speed) {
		newspeed /= speed;
		mVelocity *= newspeed;
	}
}

bool CharacterMovement::CheckGrounded() {
	if (!mUPhysicsEngine)
		return false;

	auto* collider = mOwner->GetComponent<ColliderComponent>();
	if (!collider)
		return false;

	/* 開始位置を 2 Hammer Unit (= 2HU) 上げて、自分のコライダーとの干渉を避ける */
	Vec3 startPos = mScene->GetWorldPos();
	startPos.y += Math::HtoM(2.0f); // 2HU → m

	constexpr float kCastDistHU = 1.0f; // 1HU = 1 Hammer Unit
	const float     kCastDist   = Math::HtoM(kCastDistHU); // → m
	const Vec3      kCastDir    = Vec3::down; // (0, -1, 0)

	/* UPhysics::Box を組み立てる（高さは 2HU 分） */
	Unnamed::Box box;
	box.center   = startPos;
	box.halfSize = {
		32.0f,
		Math::HtoM(2.0f), // 2HU → m
		32.0f
	};

	/* BoxCast */
	UPhysics::Hit hit{};
	bool hasHit = mUPhysicsEngine->BoxCast(box, kCastDir, kCastDist, &hit);

	if (hasHit) {
		// hit.t が 0‥1 比率の場合は距離へ換算
		float hitDistance = hit.t * kCastDist; // 距離返す仕様なら “*kCastDist” を削除

		// 接地条件：法線が十分上向き & ヒット距離がキャスト範囲内
		if (hit.normal.y > 0.7f &&
			hitDistance <= kCastDist + 1e-6f) {
			mGroundNormal = hit.normal;
			return true;
		}
	}
	return false;
}

void CharacterMovement::Accelerate(const Vec3  dir, const float speed,
                                   const float accel) {
	float currentspeed = Math::MtoH(mVelocity).Dot(dir);
	float addspeed     = speed - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed       = (std::min)(accelspeed, addspeed);
	mVelocity += Math::HtoM(accelspeed) * dir;
}

void CharacterMovement::AirAccelerate(const Vec3  dir, const float speed,
                                      const float accel) {
	float wishspd = speed;

	wishspd            = (std::min)(wishspd, 30.0f);
	float currentspeed = Math::MtoH(mVelocity).Dot(dir);
	float addspeed     = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed       = (std::min)(accelspeed, addspeed);
	mVelocity += Math::HtoM(accelspeed) * dir;
}

bool CharacterMovement::IsGrounded() const {
	return mIsGrounded;
}

void CharacterMovement::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		float       maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
			GetValueAsFloat();

		if (mVelocity[i] > Math::HtoM(maxVel)) {
			mVelocity[i] = Math::HtoM(maxVel);
		} else if (mVelocity[i] < -Math::HtoM(maxVel)) {
			mVelocity[i] = -Math::HtoM(maxVel);
		}
	}
}

Vec3 CharacterMovement::GetVelocity() const {
	return mVelocity;
}

Vec3 CharacterMovement::GetHeadPos() const {
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mCurrentHeightHu - 9.0f);
}

void CharacterMovement::SetUPhysicsEngine(UPhysics::Engine* uPhysicsEngine) {
	mUPhysicsEngine = uPhysicsEngine;
}
