#include "CharacterMovement.h"

#include <algorithm>
#include <string>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <Lib/Console/ConVarManager.h>
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


bool CharacterMovement::CheckGrounded() const {
	ColliderComponent* collider = owner_->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			"CharacterMovement::CheckGrounded() : ColliderComponent is not attached to the owner entity.",
			Vec4::yellow,
			Channel::Physics
		);
		return false;
	}

	Vec3 currentPosition = transform_->GetWorldPos();
	Vec3 direction = Vec3::down;
	float distance = Math::HtoM(4.0f); // 少し下方にキャスト
	Vec3 halfSize = collider->GetBoundingBox().GetSize();

	std::vector<HitResult> hits = collider->BoxCast(currentPosition, direction, distance, halfSize);

	for (const auto& hit : hits) {
		if (hit.isHit && hit.hitNormal.y > 0.7f) { // 上向きの法線
			return true;
		}
	}

	return false;
}

Vec3 CharacterMovement::CollideAndSlide(const Vec3& vel, const Vec3& pos, int depth) {
	vel, pos, depth;

	auto* collider = owner_->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			"CharacterMovement::CollideAndSlide() : ColliderComponent is not attached to the owner entity.",
			Vec4::yellow,
			Channel::Physics
		);
		return Vec3::zero;
	}

	constexpr int kMaxBounces = 4;
	constexpr float kMinMoveDistance = 0.01f;
	constexpr float kPushOffset = 0.003f;

	// 速度に時間を掛けて実際の移動量を計算
	Vec3 remainingVelocity = velocity_ * deltaTime_;
	Vec3 currentPosition = transform_->GetWorldPos();

	for (int i = 0; i < kMaxBounces && remainingVelocity.Length() > kMinMoveDistance; ++i) {
		// BoxCastで衝突判定
		std::vector<HitResult> hits = collider->BoxCast(
			currentPosition,
			remainingVelocity.Normalized(),
			remainingVelocity.Length(),
			collider->GetBoundingBox().GetHalfSize()
		);

		if (hits.empty()) {
			// 衝突がない場合は移動を適用
			currentPosition += remainingVelocity;
			break;
		}

		// 最も近い衝突点を見つける
		float nearestDist = FLT_MAX;
		HitResult* nearestHit = nullptr;

		for (auto& hit : hits) {
			float dist = (hit.hitPos - currentPosition).Length();
			if (dist < nearestDist) {
				nearestDist = dist;
				nearestHit = &hit;
			}
		}

		if (nearestHit) {
			// 衝突面に沿って滑らせる
			Vec3 normal = nearestHit->hitNormal;

			// 面に沿った移動ベクトルを計算
			float backoffScale = remainingVelocity.Dot(normal);
			Vec3 slideVector = remainingVelocity - (normal * backoffScale);

			// 衝突位置まで移動 + 微小オフセット
			currentPosition = nearestHit->hitPos + (normal * kPushOffset);

			// 残りの移動量を更新
			remainingVelocity = slideVector;

			// 上向きの法線との衝突で接地判定
			if (normal.y > 0.7f) {
				isGrounded_ = true;
			}
		}
	}

	// 最終位置を適用
	transform_->SetWorldPos(currentPosition);
	// 実際の速度を保持（deltaTimeで割って元のスケールに戻す）
	velocity_ = remainingVelocity / deltaTime_;

	return Vec3::zero;
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