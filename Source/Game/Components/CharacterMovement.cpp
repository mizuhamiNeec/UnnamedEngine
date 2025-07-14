#include "CharacterMovement.h"

#include <algorithm>
#include <string>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>

#include "Components/ColliderComponent/Base/ColliderComponent.h"

#include "Physics/PhysicsEngine.h"

CharacterMovement::~CharacterMovement() {
}

void CharacterMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	transform_ = owner_->GetTransform();
}

void CharacterMovement::Update(const float deltaTime) {
	deltaTime_ = deltaTime;
	position_  = transform_->GetLocalPos();

	Move();
	
	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f,
	                 Vec4::yellow);

	const float width  = Math::HtoM(mCurrentWidthHU);
	const float height = Math::HtoM(mCurrentHeightHU);
	Debug::DrawBox(
		transform_->GetWorldPos() + (Vec3::up * height * 0.5f),
		Quaternion::Euler(Vec3::zero),
		Vec3(width, height, width),
		bIsGrounded ? Vec4::green : Vec4::red);
}

void CharacterMovement::DrawInspectorImGui() {
}

void CharacterMovement::Move() {
}

void CharacterMovement::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();
	velocity_.y -= Math::HtoM(gravity) * 0.5f * deltaTime_;
}

void CharacterMovement::ApplyFriction(const float fricValue) {
	Vec3 vel = Math::MtoH(velocity_);

	vel.y       = 0.0f;
	float speed = vel.Length();
	if (speed < 0.1f) {
		return;
	}

	float drop = 0.0f;
	if (bIsGrounded) {
		const float friction  = fricValue;
		const float stopspeed = ConVarManager::GetConVar("sv_stopspeed")->
			GetValueAsFloat();
		const float control = speed < stopspeed ? stopspeed : speed;
		drop                = control * friction * deltaTime_;
	}

	float newspeed = speed - drop;

	newspeed = std::max<float>(newspeed, 0);

	if (newspeed != speed) {
		newspeed /= speed;
		velocity_ *= newspeed;
	}
}

bool CharacterMovement::CheckGrounded() {
	auto* collider = owner_->GetComponent<ColliderComponent>();
	if (!collider) {
		return false;
	}

	// 足元判定の開始位置。自分自身との衝突を避けるために少し上にオフセット
	Vec3 pos = transform_->GetWorldPos();
	pos.y += Math::HtoM(2.0f);

	constexpr float castDist = 0.01f;
	// ColliderComponentに実装してあるBoxCastを利用
	auto hitResults = collider->BoxCast(
		pos,
		Vec3::down,
		castDist,
		{
			collider->GetBoundingBox().GetHalfSize().x,
			Math::HtoM(2.0f),
			collider->GetBoundingBox().GetHalfSize().z
		}
	);

	//Debug::DrawRay(pos, Vec3::down * rayDistance, Vec4::red);

	// 各HitResultをチェックし、上向きの法線なら接地
	for (const auto& hit : hitResults) {
		if (hit.isHit && hit.hitNormal.y > 0.7f) {
			mGroundNormal = hit.hitNormal;
			return true;
		}
	}

	return false;
}

void CharacterMovement::Accelerate(const Vec3  dir, const float speed,
                                   const float accel) {
	float currentspeed = Math::MtoH(velocity_).Dot(dir);
	float addspeed     = speed - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * deltaTime_ * speed;
	accelspeed       = (std::min)(accelspeed, addspeed);
	velocity_ += Math::HtoM(accelspeed) * dir;
}

void CharacterMovement::AirAccelerate(const Vec3  dir, const float speed,
                                      const float accel) {
	float wishspd = speed;

	wishspd            = (std::min)(wishspd, 30.0f);
	float currentspeed = Math::MtoH(velocity_).Dot(dir);
	float addspeed     = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * deltaTime_ * speed;
	accelspeed       = (std::min)(accelspeed, addspeed);
	velocity_ += Math::HtoM(accelspeed) * dir;
}

bool CharacterMovement::IsGrounded() const {
	return bIsGrounded;
}

void CharacterMovement::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		float       maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
			GetValueAsFloat();

		if (velocity_[i] > Math::HtoM(maxVel)) {
			velocity_[i] = Math::HtoM(maxVel);
		} else if (velocity_[i] < -Math::HtoM(maxVel)) {
			velocity_[i] = -Math::HtoM(maxVel);
		}
	}
}

Vec3 CharacterMovement::GetVelocity() const {
	return velocity_;
}

Vec3 CharacterMovement::GetHeadPos() const {
	return transform_->GetWorldPos() + Vec3::up * Math::HtoM(
		mCurrentHeightHU - 9.0f);
}
