#include "PlayerMovement.h"

#include <Entity/Base/Entity.h>
#include <Input/InputSystem.h>
#include <Lib/Math/Vector/Vec3.h>

#include <algorithm>

#include "Camera/CameraManager.h"
#include "Components/CameraComponent.h"
#include "Debug/Debug.h"
#include "Lib/Console/ConVarManager.h"

PlayerMovement::~PlayerMovement() {}

void PlayerMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	// 初期化処理
	// トランスフォームコンポーネントを取得
	transform_ = owner_->GetTransform();
}

void PlayerMovement::ProcessInput() {
	//-------------------------------------------------------------------------
	// 移動入力
	//-------------------------------------------------------------------------
	moveInput_ = { 0.0f, 0.0f, 0.0f };

	if (InputSystem::IsPressed("forward")) {
		moveInput_.z += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		moveInput_.z -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		moveInput_.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		moveInput_.x -= 1.0f;
	}

	moveInput_.Normalize();
}

void PlayerMovement::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	velocity_.y -= Math::HtoM(gravity) * 0.5f * deltaTime_;
}

void PlayerMovement::ApplyFriction() {
	Vec3 vel = Math::MtoH(velocity_);

	vel.y = 0.0f;

	const float speed = vel.Length();

	if (speed < 0.0f) {
		return;
	}

	float drop = 0.0f;

	if (isGrounded_) {
		const float friction = ConVarManager::GetConVar("sv_friction")->GetValueAsFloat();
		const float control = (speed < ConVarManager::GetConVar("sv_stopspeed")->GetValueAsFloat()) ? ConVarManager::GetConVar("sv_stopspeed")->GetValueAsFloat() : speed;
		drop += control * friction * deltaTime_;
	}

	float newspeed = speed - drop;

	newspeed = max(newspeed, 0.0f);

	if (newspeed != speed) {
		newspeed /= speed;
	}

	velocity_ *= newspeed;
}

void PlayerMovement::FullWalkMove() {
	// 接地判定
	isGrounded_ = CheckGrounded();

	if (!isGrounded_) {
		ApplyHalfGravity();
	}

	if (isGrounded_) {
		ApplyFriction();
	}

	// ジャンプ処理
	if (isGrounded_ && InputSystem::IsPressed("+jump")) {
		velocity_.y = Math::HtoM(jumpVel_);
		isGrounded_ = false;
	}

	// CheckVelocity
	CheckVelocity();

	// ビュー行列の逆行列を計算
	Mat4 inverseViewMatrix = CameraManager::GetActiveCamera()->GetViewMat().Inverse();

	// 逆行列の各列を取得
	Vec3 right = { inverseViewMatrix.m[0][0], inverseViewMatrix.m[0][1], inverseViewMatrix.m[0][2] };
	Vec3 up = { inverseViewMatrix.m[1][0], inverseViewMatrix.m[1][1], inverseViewMatrix.m[1][2] };
	Vec3 forward = { inverseViewMatrix.m[2][0], inverseViewMatrix.m[2][1], inverseViewMatrix.m[2][2] };

	// 方向ベクトルを正規化
	right.Normalize();
	up.Normalize();
	forward.Normalize();

	if (isGrounded_) {
		Vec3 wishvel = moveInput_.x * right + moveInput_.z * forward;
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
		Vec3 wishvel = moveInput_.x * right + moveInput_.z * forward;
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

	// 入力処理
	ProcessInput();

	if (!isGrounded_) {
		ApplyHalfGravity();
	}
}

void PlayerMovement::Update([[maybe_unused]] const float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	FullWalkMove();

	transform_->SetLocalPos(position_ + velocity_ * deltaTime_);
	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), 2.0f, 0.5f, isGrounded_ ? Vec4::green : Vec4::red);
	Debug::DrawRay(transform_->GetWorldPos(), moveInput_, Vec4::cyan);
}

void PlayerMovement::DrawInspectorImGui() {

}

bool PlayerMovement::CheckGrounded() const {
	// y座標が0以下の場合は接地していると判定
	return transform_->GetLocalPos().y <= 0.0f;
}

void PlayerMovement::Accelerate(const Vec3 wishdir, const float wishspeed, const float accel) {
	float currentspeed = Math::MtoH(velocity_).Dot(wishdir);
	float addspeed = wishspeed - currentspeed;

	if (addspeed <= 0) {
		return;
	}

	float accelspeed = accel * deltaTime_ * wishspeed;

	accelspeed = min(accelspeed, addspeed);

	velocity_ += Math::HtoM(accelspeed) * wishdir;
}

void PlayerMovement::AirAccelerate([[maybe_unused]] Vec3 wishdir, [[maybe_unused]] float wishspeed, [[maybe_unused]] float accel) {
	float wishspd = wishspeed;
	if (wishspd > 30.0f) {
		wishspd = 30.0f;
	}

	float currentspeed = Math::MtoH(velocity_).Dot(wishdir);
	float addspeed = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * wishspeed * deltaTime_;

	if (accelspeed > addspeed) {
		accelspeed = addspeed;
	}

	velocity_ += Math::HtoM(accelspeed) * wishdir;
}

void PlayerMovement::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->GetValueAsFloat();

		if (velocity_[i] > Math::HtoM(maxVel)) {
			velocity_[i] = Math::HtoM(maxVel);
			Console::Print(name + "Got a velocity too high ( >" + std::to_string((Math::MtoH(velocity_[i]))) + ") on " + StrUtils::DescribeAxis(i));
		} else if (velocity_[i] < -Math::HtoM(maxVel)) {
			velocity_[i] = -Math::HtoM(maxVel);
			Console::Print(name + "Got a velocity too low ( <" + std::to_string((Math::MtoH(velocity_[i]))) + ") on " + StrUtils::DescribeAxis(i));
		}
	}
}

Vec3 PlayerMovement::GetVelocity() const {
	return velocity_;
}

