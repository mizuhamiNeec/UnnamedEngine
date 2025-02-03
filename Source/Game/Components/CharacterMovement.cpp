#include "CharacterMovement.h"

#include <algorithm>
#include <string>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>

#include "Components/ColliderComponent/Base/ColliderComponent.h"

#include "Physics/PhysicsEngine.h"

CharacterMovement::~CharacterMovement() {}

void CharacterMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void CharacterMovement::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	Move();

	position_.y = std::max<float>(position_.y, 0.0f);

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
	if (speed < 0.1f) {
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
	pos.y += collider->GetBoundingBox().GetHalfSize().y;

	constexpr float rayDistance = 0.025f;
	// ColliderComponentに実装してあるBoxCastを利用
	auto hitResults = collider->BoxCast(pos, Vec3::down, rayDistance, collider->GetBoundingBox().GetHalfSize());

	//Debug::DrawRay(pos, Vec3::down * rayDistance, Vec4::red);

	// 各HitResultをチェックし、十分な上向きの法線（地面らしい面）であれば接地と判定
	for (const auto& hit : hitResults) {
		//Debug::DrawRay(hit.hitPos, hit.hitNormal, Vec4::magenta);
		if (hit.isHit && hit.hitNormal.y > 0.7f) {
			normal_ = hit.hitNormal;
			return true;
		}
	}
	return false;
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