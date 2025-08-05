#include "game/public/components/CharacterMovement.h"

#include "engine/public/Components/ColliderComponent/Base/ColliderComponent.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/OldConsole/ConVarManager.h"
#include "engine/public/Physics/PhysicsEngine.h"
#include "engine/public/uphysics/UPhysics.h"

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
	if (!mUPhysicsEngine) return false;
	auto* collider = mOwner->GetComponent<ColliderComponent>();
	if (!collider) return false;

	constexpr float kStepHu  = 18.0f; // Source 系デフォルト段差
	constexpr float kEpsHu   = 2.0f;  // 余白
	constexpr float kBlendHz = 20.0f; // 法線追従速度
	const float     castDist = Math::HtoM(kStepHu + kEpsHu);

	// キャスト開始位置＝足元を kStepHu 持ち上げた所
	Vec3 startPos = mScene->GetWorldPos();
	startPos.y += Math::HtoM(kStepHu);

	UPhysics::Box box;
	box.center = startPos;
	box.half   = {
		collider->GetBoundingBox().GetHalfSize().x + Math::HtoM(1.0f),
		Math::HtoM(kStepHu),
		collider->GetBoundingBox().GetHalfSize().z + Math::HtoM(1.0f)
	};

	UPhysics::Hit hit{};
	if (!mUPhysicsEngine->BoxCast(box, Vec3::down, castDist, &hit))
		return false;

	if (hit.normal.y < 0.7f) // 傾斜が急すぎる
		return false;

	// 法線を時間的にブレンドして滑らかに
	float a       = 1.0f - std::exp(-kBlendHz * mDeltaTime);
	mGroundNormal = Math::Lerp(mGroundNormal, hit.normal, a).Normalized();

	// 足元にスナップ（省きたい場合はコメントアウト）
	Vec3 pos = mScene->GetWorldPos();
	pos.y    = hit.pos.y;
	mScene->SetWorldPos(pos);

	return true;
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
