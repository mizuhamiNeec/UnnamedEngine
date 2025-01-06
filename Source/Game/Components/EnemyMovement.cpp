#include "EnemyMovement.h"

#include "Debug/Debug.h"
#include "Entity/Base/Entity.h"
#include "Lib/Console/ConVarManager.h"

EnemyMovement::~EnemyMovement() {
}

void EnemyMovement::OnAttach(Entity& owner) {
	CharacterMovement::OnAttach(owner);
	transform_ = owner_->GetTransform();
	speed_ = 100.0f;
}

void EnemyMovement::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	Move();

	transform_->SetLocalPos(position_ + velocity_ * deltaTime_);

	// 範囲外に出ないように制限
	transform_->SetWorldPos(transform_->GetWorldPos().Clamp(Vec3(-32.0f, 0.0f, -32.0f), Vec3(32.0f, 100.0f, 32.0f)));

	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), 2.0f, 0.5f, isGrounded_ ? Vec4::green : Vec4::red);
}

void EnemyMovement::DrawInspectorImGui() {
	CharacterMovement::DrawInspectorImGui();
}

void EnemyMovement::Move() {
	// 接地判定
	isGrounded_ = CheckGrounded();

	if (!isGrounded_) {
		ApplyHalfGravity();
	}

	if (isGrounded_) {
		ApplyFriction();
	}

	// CheckVelocity
	CheckVelocity();

	if (isGrounded_) {
		velocity_.y = 0.0f;
		Vec3 wishvel = moveInput_;
		wishvel.y = 0.0f;
		wishvel.Normalize();
		Vec3 wishdir = wishvel;
		float wishspeed = wishdir.Length();
		wishspeed *= speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if ((wishspeed != 0.0f) && wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
			wishspeed = maxspeed;
		}
		Accelerate(wishdir, wishspeed, ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat());
	} else {
		Vec3 wishvel = moveInput_;
		wishvel.y = 0.0f;
		Vec3 wishdir = wishvel;
		wishvel.Normalize();
		float wishspeed = wishdir.Length();
		wishspeed *= speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if ((wishspeed != 0.0f) && wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
			wishspeed = maxspeed;
		}
		AirAccelerate(wishdir, wishspeed, ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat());
	}

	// y座標が0以下の場合は0にする
	position_.y = max(position_.y, 0.0f);

	if (!isGrounded_) {
		ApplyHalfGravity();
	}
}

void EnemyMovement::SetMoveInput(const Vec3 newMoveInput) {
	moveInput_ = newMoveInput.Normalized();
}
