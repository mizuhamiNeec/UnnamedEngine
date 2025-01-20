#include "PlayerMovement.h"

#include <algorithm>

#include <Camera/CameraManager.h>

#include <Components/Camera/CameraComponent.h>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <Input/InputSystem.h>

#include <Lib/Console/ConVarManager.h>
#include <Lib/Math/Vector/Vec3.h>

PlayerMovement::~PlayerMovement() {}

void PlayerMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	// 初期化処理
	// トランスフォームコンポーネントを取得
	transform_ = owner_->GetTransform();

	speed_ = 300.0f;
	jumpVel_ = 300.0f;
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

void PlayerMovement::Update([[maybe_unused]] const float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	// 入力処理
	ProcessInput();

	Move();

	transform_->SetLocalPos(position_ + velocity_ * deltaTime_);

	// 各軸の範囲外に出たら制限し、各軸のvelocityを0にする
	Vec3 worldPos = transform_->GetWorldPos();

	if (worldPos.x > 32.0f || worldPos.x < -32.0f) {
		worldPos.x = std::clamp(worldPos.x, -32.0f, 32.0f);
		velocity_.x = 0.0f;
	}

	if (worldPos.z > 32.0f || worldPos.z < -32.0f) {
		worldPos.z = std::clamp(worldPos.z, -32.0f, 32.0f);
		velocity_.z = 0.0f;
	}

	transform_->SetWorldPos(worldPos);

	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), Math::HtoM(73), Math::HtoM(33.0f * 0.5f), isGrounded_ ? Vec4::green : Vec4::red);
	Debug::DrawRay(transform_->GetWorldPos(), moveInput_, Vec4::cyan);
}

void PlayerMovement::DrawInspectorImGui() {

}

void PlayerMovement::Move() {
	// 接地判定
	isGrounded_ = CheckGrounded();

	if (!isGrounded_) {
		ApplyHalfGravity();
	}

	if (isGrounded_) {
		ApplyFriction();
	}

	// ジャンプ処理
	if (isGrounded_ && InputSystem::IsTriggered("+jump")) {
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
		velocity_.y = 0.0f;
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

	if (!isGrounded_) {
		ApplyHalfGravity();
	}
}

bool PlayerMovement::CheckGrounded() {
	// y座標が0以下の場合は接地していると判定
	return transform_->GetLocalPos().y <= 0.0f;
}

void PlayerMovement::SetVelocity(const Vec3 newVel) {
	velocity_ = newVel;
}