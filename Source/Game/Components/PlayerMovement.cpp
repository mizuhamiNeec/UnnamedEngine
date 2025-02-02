#include "PlayerMovement.h"

#include <algorithm>

#include <Camera/CameraManager.h>

#include <Components/Camera/CameraComponent.h>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <Input/InputSystem.h>

#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/Vector/Vec3.h>

#include "Components/ColliderComponent/Base/ColliderComponent.h"

#include "Physics/PhysicsEngine.h"

PlayerMovement::~PlayerMovement() {}

void PlayerMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	// 初期化処理
	// トランスフォームコンポーネントを取得
	transform_ = owner_->GetTransform();

	speed_ = 300.0f;
	jumpVel_ = 300.0f;

	//Console::SubmitCommand("sv_gravity 0");
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

	position_.y = std::max<float>(position_.y, 0.0f);
	transform_->SetLocalPos(position_);
	// デバッグ描画
	Debug::DrawArrow(transform_->GetWorldPos(), velocity_, Vec4::yellow);

	float width = Math::HtoM(33.0f);
	float height = Math::HtoM(73.0f);
	Debug::DrawBox(
		transform_->GetWorldPos() + (Vec3::up * height * 0.5f),
		Quaternion::Euler(Vec3::zero),
		Vec3(width, height, width),
		isGrounded_ ? Vec4::green : Vec4::blue
	);
	Debug::DrawRay(transform_->GetWorldPos(), wishdir_, Vec4::cyan);
}

static Vec3 test;

void PlayerMovement::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("PlayerMovement", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGuiManager::DragVec3("Velocity", velocity_, 0.1f, "%.2f m/s");
		ImGuiManager::DragVec3("test", test, 0.1f, "%.2f m/s");
	}
#endif
}

void PlayerMovement::Move() {
	if (!isGrounded_) {
		ApplyHalfGravity();
	}

	isGrounded_ = CheckGrounded();

	// ジャンプ処理
	if (isGrounded_ && InputSystem::IsPressed("+jump")) {
		velocity_.y = Math::HtoM(jumpVel_);
		isGrounded_ = false; // ジャンプ中は接地判定を解除
	}

	//if (isGrounded_ && velocity_.y < 0.0f) {
	//	velocity_.y = 0.0f; // 落下中のみリセット
	//}

	if (isGrounded_) {
		ApplyFriction();
	}

	// CheckVelocity
	CheckVelocity();

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();
	cameraForward.y = 0.0f; // Y成分を0にする
	cameraForward.Normalize();

	// カメラの右方向ベクトルを計算（Y軸との外積）
	Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

	// 入力に基づいて移動方向を計算
	Vec3 wishvel = (cameraForward * moveInput_.z) + (cameraRight * moveInput_.x);
	wishvel.y = 0.0f;

	if (!wishvel.IsZero()) {
		wishvel.Normalize();
	}

	if (isGrounded_) {
		wishdir_ = wishvel;
		float wishspeed = wishdir_.Length() * speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		Accelerate(wishdir_, wishspeed, ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat());
	} else {
		wishdir_ = wishvel;
		float wishspeed = wishdir_.Length() * speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		AirAccelerate(wishdir_, wishspeed, ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat());
	}

	position_ += velocity_ * deltaTime_;

	//test = CollideAndSlide(velocity_, transform_->GetWorldPos(), 0); // 衝突判定とスライド移動
	//velocity_
	//isGrounded_ = CheckGrounded(); // 最後に接地判定を更新

	if (!isGrounded_) {
		ApplyHalfGravity();
	}
}

void PlayerMovement::SetIsGrounded(bool bIsGrounded) {
	isGrounded_ = bIsGrounded;
}

Vec3 PlayerMovement::CollideAndSlide(const Vec3& vel, const Vec3& pos, int depth) {
	auto* collider = owner_->GetComponent<ColliderComponent>();

	if (!collider) {
		Console::Print(
			"PlayerMovement::CollideAndSlide() : ColliderComponent is not attached to the owner entity.",
			Vec4::yellow,
			Channel::Physics
		);
		return Vec3::zero;
	}

	if (depth >= maxBounces) {
		return Vec3::zero;
	}

	float dist = vel.Length() + skinWidth;

	std::vector<HitResult> hits = collider->BoxCast(pos, vel.Normalized(), dist, collider->GetBoundingBox().GetHalfSize());;

	if (hits.empty()) {
		return vel;
	}

	if (hits.size() < 2) {
		return vel;
	}

	if (hits[1].isHit) {
		HitResult hit = hits[1];
		Debug::DrawBox(hit.hitPos, Quaternion::identity, collider->GetBoundingBox().GetHalfSize(), Vec4::orange);
		Console::Print("Hit!" + hit.hitPos.ToString(), Vec4::red, Channel::Physics);

		Vec3 snapToSurface = vel.Normalized() * (hit.dist - skinWidth);
		Vec3 leftover = vel - snapToSurface;

		if (snapToSurface.Length() <= skinWidth) {
			snapToSurface = Vec3::zero;
		}

		float mag = leftover.Length();
		if (mag < 0.1f) {
			return Vec3::zero;
		}
		// ノーマルに沿って投影
		leftover = leftover - hit.hitNormal * leftover.Dot(hit.hitNormal);
		leftover = leftover.Normalized() * mag;

		return snapToSurface + CollideAndSlide(leftover, pos + snapToSurface, depth + 1);
	}

	return vel;
}

void PlayerMovement::SetVelocity(const Vec3 newVel) {
	velocity_ = newVel;
}
