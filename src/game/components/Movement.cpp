#include "Movement.h"

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/Transform/SceneComponent.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>

#include <algorithm>

void Movement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

/// @brief Movementコンポーネントを初期化します
/// @param uphysics UPhysicsエンジンのポインタ
/// @param movementData MovementData構造体への参照
void Movement::Init(UPhysics::Engine*   uphysics,
                    const MovementData& movementData) {
	mUPhysicsEngine = uphysics;
	mData           = movementData;
}

void Movement::PrePhysics(float deltaTime) {
	Component::PrePhysics(deltaTime);

	// 原点は足元
	Vec3 boxCenter = mScene->GetWorldPos() +
		Vec3::up * Math::HtoM(mData.currentHeight * 0.5f);

	Debug::DrawBox(
		boxCenter,
		Quaternion::identity,
		Math::HtoM(
			{mData.currentWidth, mData.currentHeight, mData.currentWidth}),
		{0.24f, 0.57f, 0.96f, 1.0f}
	);
}

void Movement::Update(float deltaTime) {
	ProcessInput();
	ProcessMovement(deltaTime);
}

void Movement::PostPhysics(float deltaTime) {
	Component::PostPhysics(deltaTime);

	Debug::DrawArrow(
		mScene->GetLocalPos(),
		mData.velocity * 0.25f,
		Vec4::yellow
	);

	// 移動させる
	mScene->SetLocalPos(mScene->GetLocalPos() + mData.velocity * deltaTime);
}

void Movement::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("CurrentState: %s", ToString(mData.state));
		ImGuiWidgets::DragVec3(
			"vel", mData.velocity, Vec3::zero, 0.1f, "%.2f m/s"
		);
	}
#endif
}

/// @brief 現在の速度を取得します
/// @return 速度ベクトル
Vec3& Movement::GetVelocity() {
	return mData.velocity;
}

/// @brief 頭の位置を取得します
/// @return 頭の位置（ワールド座標）
Vec3 Movement::GetHeadPos() const {
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mData.currentHeight - 9.0f);
}

/// @brief 速度を設定します
/// @param vel 速度ベクトル
void Movement::SetVelocity(const Vec3& vel) { mData.velocity = vel; }

/// @brief 入力処理を行います
void Movement::ProcessInput() {
	mData.vecMoveInput = Vec2::zero;

	// 移動入力
	// 同時押ししていたら打ち消すように
	if (InputSystem::IsPressed("forward")) {
		mData.vecMoveInput.y += 1.0f;
	}
	if (InputSystem::IsPressed("back")) {
		mData.vecMoveInput.y -= 1.0f;
	}
	if (InputSystem::IsPressed("moveright")) {
		mData.vecMoveInput.x += 1.0f;
	}
	if (InputSystem::IsPressed("moveleft")) {
		mData.vecMoveInput.x -= 1.0f;
	}
	mData.vecMoveInput.Normalize(); // 正規化しておく

	// ジャンプ・しゃがみ入力
	mData.wishJump   = InputSystem::IsPressed("jump");
	mData.wishCrouch = InputSystem::IsPressed("crouch");

	// プレイヤーの向いている方向を計算
	const auto camera = CameraManager::GetActiveCamera();
	if (!camera) {
		Error("Movement", "ProcessInput: カメラが存在しません");
		mData.wishDirection = Vec3::zero;
		return;
	}
	Vec3 camForward = camera->GetViewMat().Inverse().GetForward();

	camForward.y = 0.0f;
	camForward.Normalize();

	const Vec3 camRight = Vec3::up.Cross(camForward).Normalized();

	mData.wishDirection =
		camForward * mData.vecMoveInput.y +
		camRight * mData.vecMoveInput.x;
	mData.wishDirection.y = 0.0f; // 移動に使うのは水平成分のみ
	if (!mData.wishDirection.IsZero()) {
		mData.wishDirection.Normalize(); // 正規化
	}

	// 入力方向を可視化
	Debug::DrawArrow(
		mScene->GetWorldPos(),
		mData.wishDirection * 0.5f,
		Vec4::cyan,
		0.05f
	);
}

void Movement::ProcessMovement(const float dt) {
	if (mData.wishCrouch) {
		mData.currentHeight = mData.crouchHeight;
	} else {
		mData.currentHeight = mData.defaultHeight;
	}

	ApplyHalfGravity(dt);

	float wishspeed = mData.currentSpeed;

	if (
		mData.currentSpeed >
		ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat()
	) {
		wishspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
	}

	switch (mData.state) {
	case MOVEMENT_STATE::GROUND:
		Ground(wishspeed, dt);
		break;
	case MOVEMENT_STATE::AIR:
		Air(wishspeed, dt);
		break;
	case MOVEMENT_STATE::WALL_RUN:
		WallRun(dt);
		break;
	case MOVEMENT_STATE::SLIDE:
		Slide(dt);
		break;
	}

	ApplyHalfGravity(dt);

	// 変な値になっていないかの確認とクランプ
	CheckForNaNAndClamp();
}

/// @brief 地上での移動処理を行います
/// @param wishspeed 目標速度
/// @param dt デルタタイム
void Movement::Ground(const float wishspeed, const float dt) {
	mData.velocity.y = 0.0f;
	Friction(dt);

	// 加速装置!
	Accelerate(
		mData.wishDirection,
		wishspeed,
		ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat(),
		dt
	);
}

/// @brief 空中での移動処理を行います
/// @param wishspeed 目標速度
/// @param dt デルタタイム
void Movement::Air(const float wishspeed, const float dt) {
	AirAccelerate(
		mData.wishDirection,
		wishspeed,
		ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat(),
		dt
	);
}

/// @brief 壁走りでの移動処理を行います
/// @param dt デルタタイム
void Movement::WallRun(const float dt) {
	(void)dt;
}

/// @brief スライディングでの移動処理を行います
/// @param dt デルタタイム
void Movement::Slide(const float dt) {
	(void)dt;
}

/// @brief 重力の半分を適用します
/// @param dt deltaTime
void Movement::ApplyHalfGravity(const float dt) {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();
	mData.velocity.y -= Math::HtoM(gravity) * 0.5f * dt;
}

/// @brief 摩擦処理を行います
/// @param dt deltaTime
void Movement::Friction(const float dt) {
	const float speed = Math::MtoH(mData.velocity.Length());
	if (speed < 0.1f) { return; }
	float drop = 0;
	// 接地していたら
	if (true) {
		const float friction = ConVarManager::GetConVar("sv_friction")->
			GetValueAsFloat();
		const float stopspeed = ConVarManager::GetConVar("sv_stopspeed")->
			GetValueAsFloat();
		const float control = speed < stopspeed ? stopspeed : speed;

		drop += control * friction * dt;
	}
	float newspeed = speed - drop;
	newspeed       = std::max(newspeed, 0.0f);

	if (newspeed != speed) {
		newspeed = newspeed / speed;
		mData.velocity *= newspeed;
	}
}

/// @brief 地上での加速処理を行います
/// @param dir 方向
/// @param speed 目標速度
/// @param accel 加速度
/// @param dt deltaTime
void Movement::Accelerate(
	const Vec3 dir, const float speed, const float accel, const float dt
) {
	const float currentspeed = Math::MtoH(mData.velocity).Dot(dir);
	const float addspeed     = speed - currentspeed;
	if (addspeed <= 0.0f) { return; }
	float accelspeed = accel * speed * dt;
	accelspeed       = std::min(accelspeed, addspeed);
	mData.velocity += Math::HtoM(accelspeed) * dir;
}

/// @brief 空中での加速処理を行います
/// @param dir 方向
/// @param speed 目標速度
/// @param accel 加速度
/// @param dt deltaTime
void Movement::AirAccelerate(
	const Vec3 dir, const float speed, const float accel, const float dt
) {
	float wishspd            = speed;
	wishspd                  = std::min(wishspd, kAirSpeedCap);
	const float currentspeed = Math::MtoH(mData.velocity).Dot(dir);
	const float addspeed     = wishspd - currentspeed;
	if (addspeed <= 0.0f) { return; }
	float accelspeed = accel * speed * dt;
	accelspeed       = std::min(accelspeed, addspeed);
	// メートルに変換して加算
	mData.velocity += Math::HtoM(accelspeed) * dir;
}

/// @brief 移動してみる 
void Movement::TryMove() {
}

/// @brief 座標/速度が異常な値になっていないかチェックし、修正します
void Movement::CheckForNaNAndClamp() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		const float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
			GetValueAsFloat();

		// NAN チェック
		// ベロシティ
		if (isnan(mData.velocity[i])) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a NaN velocity {}",
				name, StrUtil::DescribeAxis(i)
			);
			mData.velocity[i] = 0.0f;
		}
		// 座標
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

		// 各軸の速度が最大値を超えていないかチェック->超えている場合はクランプ
		if (mData.velocity[i] > Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too high on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mData.velocity[i] = Math::HtoM(maxVel);
		} else if (mData.velocity[i] < -Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too low on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mData.velocity[i] = -Math::HtoM(maxVel);
		}
	}
}
