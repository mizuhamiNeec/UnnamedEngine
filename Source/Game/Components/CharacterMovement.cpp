#include "CharacterMovement.h"

#include <algorithm>
#include <string>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <Lib/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>

CharacterMovement::~CharacterMovement() {}

void CharacterMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void CharacterMovement::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	Move();

	transform_->SetLocalPos(position_ + velocity_ * deltaTime);
	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), Math::HtoM(73.0f), Math::HtoM(33.0f * 0.5f), isGrounded_ ? Vec4::green : Vec4::red);
}

void CharacterMovement::DrawInspectorImGui() {}

void CharacterMovement::Move() {}

void CharacterMovement::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	velocity_.y -= Math::HtoM(gravity) * 0.5f * deltaTime_;
}

void CharacterMovement::ApplyFriction() {
	Vec3 vel = Math::MtoH(velocity_);

	vel.y = 0.0f;
	float speed = vel.Length();
	if (speed < 0.0f) {
		return;
	}

	float drop = 0.0f;
	if (isGrounded_) {
		float friction = ConVarManager::GetConVar("sv_friction")->GetValueAsFloat();
		float stopspeed = ConVarManager::GetConVar("sv_stopspeed")->GetValueAsFloat();
		float control = speed < stopspeed ? stopspeed : speed;
		drop = control * friction * deltaTime_;
	}

	float newspeed = speed - drop;

	newspeed = std::max<float>(newspeed, 0);

	if (newspeed != speed && speed != 0.0f) {
		newspeed /= speed;
	}

	velocity_ *= newspeed;
}


bool CharacterMovement::CheckGrounded() const {
	// y座標が0以下の場合は接地していると判定
	return transform_->GetLocalPos().y <= 0.0f;
}

void CharacterMovement::Accelerate(const Vec3 dir, const float speed, const float accel) {
	float currentspeed = Math::MtoH(velocity_).Dot(dir);
	float addspeed = speed - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * deltaTime_ * speed;
	accelspeed = min(accelspeed, addspeed);
	velocity_ += Math::HtoM(accelspeed) * dir;
}

void CharacterMovement::AirAccelerate(const Vec3 dir, const float speed, const float accel) {
	float wishspd = speed;

	wishspd = min(wishspd, 30.0f);
	float currentspeed = Math::MtoH(velocity_).Dot(dir);
	float addspeed = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * deltaTime_ * speed;
	accelspeed = min(accelspeed, addspeed);
	velocity_ += Math::HtoM(accelspeed) * dir;
}

bool CharacterMovement::IsGrounded() const {
	return isGrounded_;
}

void CharacterMovement::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->GetValueAsFloat();

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