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

	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), Math::HtoM(73), Math::HtoM(33.0f * 0.5f), isGrounded_ ? Vec4::green : Vec4::red);

	Debug::DrawRay(transform_->GetWorldPos(), wishdir, Vec4::cyan);
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
	if (isGrounded_ && InputSystem::IsPressed("+jump")) {
		velocity_.y = Math::HtoM(jumpVel_);
		isGrounded_ = false;
	}

	// CheckVelocity
	CheckVelocity();

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();
	cameraForward.y = 0.0f;  // Y成分を0にする
	cameraForward.Normalize();  // XZ平面での正規化が重要

	// カメラの右方向ベクトルを計算（Y軸との外積）
	Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

	// 入力に基づいて移動方向を計算
	Vec3 wishvel = (cameraForward * moveInput_.z) + (cameraRight * moveInput_.x);
	wishvel.y = 0.0f;  // Y成分を確実に0に

	if (!wishvel.IsZero()) {
		wishvel.Normalize();
	}

	if (isGrounded_) {
		velocity_.y = 0.0f;
		wishvel.y = 0.0f;
		wishvel.Normalize();
		wishdir = wishvel;
		float wishspeed = wishdir.Length();
		wishspeed *= speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if ((wishspeed != 0.0f) && wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
			wishspeed = maxspeed;
		}
		Accelerate(wishdir, wishspeed, ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat());
	} else {
		wishvel.y = 0.0f;
		wishdir = wishvel;
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

	isGrounded_ = CheckGrounded();

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