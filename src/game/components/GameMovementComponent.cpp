#include "GameMovementComponent.h"

#include <algorithm>

#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Components/Camera/CameraComponent.h>
#include <engine/public/Components/Transform/SceneComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/ImGui/ImGuiWidgets.h>
#include <engine/public/Input/InputSystem.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/subsystem/console/Log.h>

GameMovementComponent::~GameMovementComponent() = default;

void GameMovementComponent::SetUPhysicsEngine(UPhysics::Engine* engine) {
	mUPhysicsEngine = engine;
}

/// @brief カメラに渡す用
Vec3 GameMovementComponent::GetHeadPos() const {
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mMovementState.height - 9.0f);
}

Vec3 GameMovementComponent::Velocity() const {
	return mMovementState.vecVelocity;
}

void GameMovementComponent::SetVelocity(const Vec3 newVel) {
	mMovementState.vecVelocity = newVel;
}

void GameMovementComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void GameMovementComponent::OnDetach() {
	Component::OnDetach();
}

void GameMovementComponent::PrePhysics(const float deltaTime) {
	mDeltaTime = deltaTime;
	ProcessInput();
	CalcMoveDir();
}

void GameMovementComponent::Update(float deltaTime) {
	(void)deltaTime;
}

void GameMovementComponent::PostPhysics(float) {
	// 移動を適用
	mScene->SetWorldPos(
		mScene->GetWorldPos() + mMovementState.vecVelocity * mDeltaTime
	);

	Debug::DrawArrow(
		mScene->GetWorldPos(),
		mMovementState.vecVelocity * 0.25f,
		Vec4::yellow
	);
}

void GameMovementComponent::Render(ID3D12GraphicsCommandList* commandList) {
	Component::Render(commandList);
}

void GameMovementComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("GameMovementComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SeparatorText("MovementState");

		ImGuiWidgets::DragVec3(
			"wishDir",
			mMovementState.vecWishDir,
			Vec3::zero,
			0.1f,
			"%.2f"
		);
		ImGuiWidgets::DragVec3(
			"velocity",
			mMovementState.vecVelocity,
			Vec3::zero,
			0.1f,
			"%.2f m/s"
		);

		ImGui::DragFloat(
			"height",
			&mMovementState.height,
			1.0f
		);
		ImGui::DragFloat(
			"width",
			&mMovementState.width,
			1.0f
		);

		ImGui::DragFloat(
			"currentSpeed",
			&mMovementState.currentSpeed,
			1.0f
		);

		ImGui::BeginDisabled();
		ImGui::Checkbox("IsGrounded", &mMovementState.bIsGrounded);
		ImGui::EndDisabled();
	}
#endif
}

bool GameMovementComponent::IsEditorOnly() const {
	return Component::IsEditorOnly();
}

Entity* GameMovementComponent::GetOwner() const {
	return Component::GetOwner();
}

void GameMovementComponent::ProcessInput() {
	mMoveInput.vecMoveDir = Vec2::zero;

	if (InputSystem::IsPressed("forward")) {
		mMoveInput.vecMoveDir.y += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		mMoveInput.vecMoveDir.y -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		mMoveInput.vecMoveDir.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		mMoveInput.vecMoveDir.x -= 1.0f;
	}

	mMoveInput.vecMoveDir.Normalize();

	if (InputSystem::IsPressed("jump")) {
		mMoveInput.bWishJump = true;
	}

	if (InputSystem::IsReleased("jump")) {
		mMoveInput.bWishJump = false;
	}

	if (InputSystem::IsPressed("crouch")) {
		mMoveInput.bWishCrouch = true;
	}

	if (InputSystem::IsReleased("crouch")) {
		mMoveInput.bWishCrouch = false;
	}
}

void GameMovementComponent::CalcMoveDir() {
	auto cameraInvViewMat = CameraManager::GetActiveCamera()->GetViewMat()
		.Inverse();
	Vec3 cameraForward = cameraInvViewMat.GetForward();
	Vec3 cameraRight   = cameraInvViewMat.GetRight();

	cameraForward.y = 0.0f;
	cameraRight.y   = 0.0f;
	cameraForward.Normalize();
	cameraRight.Normalize();

	mMovementState.vecWishDir =
		cameraForward * mMoveInput.vecMoveDir.y +
		cameraRight * mMoveInput.vecMoveDir.x;

	mMovementState.vecWishDir.y = 0.0f;

	if (!mMovementState.vecWishDir.IsZero()) {
		mMovementState.vecWishDir.Normalize();
	}
}

void GameMovementComponent::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();
	mMovementState.vecVelocity.y -= Math::HtoM(gravity) * 0.5f * mDeltaTime;
}

void GameMovementComponent::Friction() {
	Vec3 vel    = Math::MtoH(mMovementState.vecVelocity);
	vel.y       = 0.0f;
	float speed = vel.Length();
	if (speed < 0.1f) {
		return;
	}

	float       drop     = 0.0f;
	const float friction = ConVarManager::GetConVar("sv_friction")->
		GetValueAsFloat();
	const float stopspeed = ConVarManager::GetConVar("sv_stopspeed")->
		GetValueAsFloat();
	const float control = speed < stopspeed ? stopspeed : speed;
	drop                = control * friction * mDeltaTime;

	float newspeed = speed - drop;

	newspeed = std::max<float>(newspeed, 0.0f);

	if (newspeed != speed) {
		newspeed /= speed;
		vel *= newspeed;
	}

	mMovementState.vecVelocity = {
		Math::HtoM(vel.x),
		0.0f,
		Math::HtoM(vel.z)
	};
}

void GameMovementComponent::Accelerate(
	const Vec3& dir, float speed, float accel
) {
	float currentSpeed = Math::MtoH(mMovementState.vecVelocity).Dot(dir);
	float addspeed     = speed - currentSpeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed       = std::min<float>(accelspeed, addspeed);
	mMovementState.vecVelocity += Math::HtoM(accelspeed) * dir;
}

void GameMovementComponent::AirAccelerate(
	const Vec3& dir, float speed, float accel
) {
	float wishspd = speed;

	wishspd            = std::min<float>(wishspd, 30.0f);
	float currentspeed = Math::MtoH(mMovementState.vecVelocity).Dot(dir);
	float addspeed     = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed       = std::min<float>(accelspeed, addspeed);
	mMovementState.vecVelocity += Math::HtoM(accelspeed) * dir;
}

void GameMovementComponent::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		const float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
			GetValueAsFloat();

		if (isnan(mMovementState.vecVelocity[i])) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a NaN velocity {}",
				name, StrUtil::DescribeAxis(i)
			);
			mMovementState.vecVelocity[i] = 0.0f;
		}

		if (isnan(mScene->GetWorldPos()[i])) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a NaN position on {}",
				name, StrUtil::DescribeAxis(i)
			);
			Vec3 pos = mScene->GetWorldPos();
			pos[i]   = 0.0f;
			mScene->SetWorldPos(pos);
		}

		if (mMovementState.vecVelocity[i] > Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too high on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mMovementState.vecVelocity[i] = Math::HtoM(maxVel);
		} else if (mMovementState.vecVelocity[i] < -Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too low on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mMovementState.vecVelocity[i] = -Math::HtoM(maxVel);
		}
	}
}
