#include "MovementComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Components/Transform/SceneComponent.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>

#include "engine/unnamed/subsystem/console/concommand/UnnamedConVar.h"

#include <engine/Engine.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>

#include "KinematicCollisionResolver.h"
#include "states/AirMove.h"
#include "states/GroundMove.h"
#include "states/PlayerMovementStateMachine.h"
#include "states/SlideMove.h"
#include "states/WallrunMove.h"

static constexpr std::string_view kChannel = "MovementComponent";

/// @brief コンストラクタ
/// @param width プレイヤーの幅
/// @param height プレイヤーの高さ
MovementData::MovementData(float width, float height) : currentWidthHu(width),
	currentHeightHu(height) {
	defaultHeightHu = height;
	crouchHeightHu  = height * 0.6f;
}

/// @brief デフォルトコンストラクタ
MovementData::MovementData() : currentWidthHu(32.0f),
                               currentHeightHu(72.0f) {
	defaultHeightHu = currentHeightHu;
	crouchHeightHu  = currentHeightHu * 0.75f;
}

/// @brief コンポーネントがエンティティにアタッチされたときの処理
/// @param owner 所有エンティティ
void MovementComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	// AABBコライダーを取得
	mCollider = owner.GetComponent<AABBCollider>();

	if (!mCollider) { Error(kChannel, "AABBColliderを取得できませんでした。"); }

	// コンソールを取得
	mConsoleSystem = ServiceLocator::Get<Unnamed::ConsoleSystem>();

	if (!mConsoleSystem) { Error(kChannel, "ConsoleSystemを取得できませんでした。"); }

	if (auto* engine = ServiceLocator::Get<Unnamed::Engine>()) {
		if (auto* audioMgr = engine->GetAudioManagerInstance()) {
			mFootstepAudio = audioMgr->GetAudio(
				"./content/parkour/sounds/se/footstep/Robot_Footstep_Single_v3_02.wav"
			);
			mLandAudio = audioMgr->GetAudio(
				"./content/parkour/sounds/se/footstep/Robot_Footstep_Single_v2_01.wav"
			);
		}
	}
}

/// @brief 初期化
/// @param uPhysics UPhysicsエンジンポインタ
/// @param md 移動データ
void MovementComponent::Init(
	UPhysics::Engine* uPhysics, const MovementData& md
) {
	mUPhysicsEngine             = uPhysics;
	mData                       = md;
	mData.velocity              = Vec3::zero;
	mData.state                 = MOVEMENT_STATE::AIR;
	mData.isGrounded            = false;
	mData.lastPosition          = Vec3::zero;
	mData.stuckTime             = 0.0f;
	mData.isStuck               = false;
	mData.isWallRunning         = false;
	mData.wallRunTime           = 0.0f;
	mData.timeSinceLastWallRun  = 999.0f;
	mData.wallRunJumpWasPressed = false;
	mData.jumpSnapDisableTime   = 0.0f;

	// 動く床トラッキングの初期化
	mLastGroundEntity   = nullptr;
	mLastGroundPosition = Vec3::zero;
	mLastGroundRotation = Quaternion::identity;
	mSurfaceVelocity    = Vec3::zero;
	mWasOnMovingSurface = false;

	UpdateHullDimensions();

	// 衝突解決器の初期化
	mCollisionResolver = std::make_unique<KinematicCollisionResolver>(
		mUPhysicsEngine, &mData, mScene, &mHull
	);
}

/// @brief 物理演算前の更新
/// @param deltaTime 経過時間
void MovementComponent::PrePhysics(float) {}

/// @brief 更新
/// @param dt 経過時間
void MovementComponent::Update(const float dt) {
	ProcessInput();

#ifdef _DEBUG
	DebugDraw::DrawBox(
		mHull.center,
		Quaternion::identity,
		mHull.halfSize * 2.0f,
		{0.34f, 0.66f, 0.95f, 1.0f}
	);
	DebugDraw::DrawArrow(
		mHull.center,
		mData.velocity * 0.25f,
		Vec4::yellow,
		0.05f
	);
#endif
	ProcessMovement(dt);
}

/// @brief 物理演算後の更新
/// @param deltaTime 経過時間
void MovementComponent::PostPhysics(float) {}

/// @brief インスペクタ内のImGui描画
void MovementComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"MovementComponent",
		ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::Text("State: %s", ToString(mData.state));
		ImGuiWidgets::DragVec3(
			"Velocity", mData.velocity, Vec3::zero, 0.1f,
			"%.3f"
		);
		ImGui::Checkbox("Grounded", &mData.isGrounded);
		ImGui::Text(
			"HeightHU: %.2f  WidthHU: %.2f", mData.currentHeightHu,
			mData.currentWidthHu
		);

		// Moving surface info
		if (mWasOnMovingSurface) {
			ImGui::Separator();
			ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "On Moving Surface");
			ImGui::Text(
				"Total Surface Vel: (%.2f, %.2f, %.2f) HU/s",
				Math::MtoH(mSurfaceVelocity.x),
				Math::MtoH(mSurfaceVelocity.y),
				Math::MtoH(mSurfaceVelocity.z)
			);
			Vec3 horizontalVel = mSurfaceVelocity;
			horizontalVel.y    = 0.0f;
			ImGui::Text(
				"Horizontal: %.2f HU/s",
				Math::MtoH(horizontalVel.Length())
			);
			ImGui::Text(
				"Magnitude: %.2f HU/s",
				Math::MtoH(mSurfaceVelocity.Length())
			);

			if (mLastGroundEntity) {
				ImGui::Text("Entity: %s", mLastGroundEntity->GetName().c_str());

				// 回転中心からの距離を表示
				Vec3 platformPos = mLastGroundEntity->GetTransform()->
					GetWorldPos();
				Vec3  toPlayer = mScene->GetWorldPos() - platformPos;
				float radius   = toPlayer.Length();
				ImGui::Text(
					"Distance from center: %.2f m (%.2f HU)", radius,
					Math::MtoH(radius)
				);
			}
		}

		// Wallrun info
		if (mData.isWallRunning) {
			ImGui::TextColored(
				{0.0f, 1.0f, 1.0f, 1.0f}, "WALLRUNNING! (%.2fs)",
				mData.wallRunTime
			);
			ImGuiWidgets::DragVec3(
				"WallNormal", mData.wallRunNormal,
				Vec3::zero, 0.1f, "%.3f"
			);
		} else {
			ImGui::Text(
				"Time since wallrun: %.2fs",
				mData.timeSinceLastWallRun
			);
		}

		if (mData.isStuck) {
			ImGui::TextColored(
				{1.0f, 0.0f, 0.0f, 1.0f}, "STUCK! (%.2fs)",
				mData.stuckTime
			);
		} else { ImGui::Text("Stuck Timer: %.2fs", mData.stuckTime); }
	}
#endif
}

/// @brief 速度を取得する
/// @return 速度ベクトル参照
Vec3& MovementComponent::GetVelocity() { return mData.velocity; }

/// @brief 頭の位置を取得する
/// @return 頭の位置
Vec3 MovementComponent::GetHeadPos() const {
	// 足元原点前提：頭は currentHeightHU から少し下げる
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		       mData.currentHeightHu - 8.0f
	       );
}

/// @brief 速度を設定する
/// @param v 速度ベクトル
void MovementComponent::SetVelocity(const Vec3& v) { mData.velocity = v; }

/// @brief 接地しているかを取得する
/// @return 接地しているならtrueを返す
bool MovementComponent::IsGrounded() const { return mData.isGrounded; }

/// @brief 移動入力ベクトルを取得する
/// @return 移動入力ベクトル参照
bool MovementComponent::WishJump() const { return mData.wishJump; }

/// @brief 壁走り中かを取得する
/// @return 壁走り中ならtrueを返す
bool MovementComponent::IsWallRunning() const { return mData.isWallRunning; }

/// @brief スライディング中かを取得する
/// @return スライディング中ならtrueを返す
bool MovementComponent::IsSliding() const { return mData.isSliding; }

/// @brief ダブルジャンプが可能かを取得する
/// @return ダブルジャンプ可能ならtrueを返す
bool MovementComponent::HasDoubleJump() const { return mData.hasDoubleJump; }

/// @brief 壁走りの法線ベクトルを取得する
/// @return 壁走りの法線ベクトル
Vec3 MovementComponent::GetWallRunNormal() const { return mData.wallRunNormal; }

/// @brief 接地した直後かを取得する
/// @return 接地直後ならtrueを返す
bool MovementComponent::JustLanded() const { return mData.justLanded; }

/// @brief 最後に接地したときのY方向速度を取得する
/// @return Y方向速度
float MovementComponent::GetLastLandingVelocityY() const {
	return mData.lastLandingVelocityY;
}

bool MovementComponent::IsDucking() const { return mData.wishCrouch; }

/// @brief 入力処理
void MovementComponent::ProcessInput() {
	mData.vecMoveInput = Vec2::zero;
	if (InputSystem::IsPressed("forward")) mData.vecMoveInput.y += 1.0f;
	if (InputSystem::IsPressed("back")) mData.vecMoveInput.y -= 1.0f;
	if (InputSystem::IsPressed("moveright")) mData.vecMoveInput.x += 1.0f;
	if (InputSystem::IsPressed("moveleft")) mData.vecMoveInput.x -= 1.0f;

	Vec2 leftStick     = InputSystem::GetLeftStick(0);
	mData.vecMoveInput += leftStick; // アナログスティック入力を足す。後でノーマライズするから問題🍐

	const float sqrLen = mData.vecMoveInput.SqrLength();
	if (sqrLen > 1.0f) {
		mData.vecMoveInput       *= 1.0f / std::sqrt(sqrLen);
		mData.moveInputIntensity = 1.0f;
	} else if (sqrLen > 1e-6f) {
		mData.moveInputIntensity = std::sqrt(sqrLen);
	} else { mData.moveInputIntensity = 0.0f; }

	Vec3 wish = Vec3::zero;
	if (const auto cam = CameraManager::GetActiveCamera()) {
		Vec3 f           = cam->GetViewMat().Inverse().GetForward();
		f.y              = 0.0f;
		const float fLen = f.Length();
		if (fLen > 1e-6f) {
			f *= 1.0f / fLen;
			const Vec3 r = Vec3::up.Cross(f).Normalized();
			wish = f * mData.vecMoveInput.y + r * mData.vecMoveInput.x;
			wish.y = 0.0f;
			const float wishLen = wish.Length();
			if (wishLen > 1e-6f) wish *= 1.0f / wishLen;
		}
	}
	mData.wishDirection = wish;
	mData.wishJump      = InputSystem::IsPressed("jump");
	mData.wishCrouch    = InputSystem::IsPressed("duck");
}

/// @brief 移動処理
/// @param dt 経過時間
void MovementComponent::ProcessMovement(const float dt) {
	// ジャンプスナップ無効時間の更新
	if (mData.jumpSnapDisableTime > 0.0f) { mData.jumpSnapDisableTime -= dt; }

	// 動く床の速度計算
	Entity* currentGroundEntity = nullptr;
	bool    isOnMovingSurface   = false;
	UpdateMovingSurface(dt, currentGroundEntity, isOnMovingSurface);

	// 前フレームの接地状態を記録
	mData.wasGroundedLastFrame = mData.isGrounded;

	// しゃがみ・高さ更新
	UpdateCrouch(dt);

	// ヒットボックス更新
	UpdateHullDimensions();

	mData.currentSpeed = mData.wishCrouch ?
		                     mData.crouchSpeed :
		                     mData.sprintSpeed;

	// スライド判定
	if (mData.isGrounded && !mData.isSliding && CanSlide()) { TryStartSlide(); }
	if (mData.isSliding) { UpdateSlide(dt); }

	// ウォールラン判定
	mData.timeSinceLastWallRun += dt;
	if (!mData.isGrounded && !mData.isWallRunning && CanWallrun()) {
		TryStartWallrun();
	}

	// ジャンプ処理
	ProcessJump(isOnMovingSurface);

	// 前フレームのジャンプ入力を保存
	mData.lastFrameWishJump = mData.wishJump;

	// 速度計算を各 State に委譲
	if (mData.isWallRunning) {
		WallrunMove wallrunState;
		wallrunState.Update(this, mData, dt);
		UpdateWallrun(dt);
	} else if (mData.isSliding) {
		SlideMove slideState;
		slideState.Update(this, mData, dt);
	} else {
		if (mData.isGrounded) {
			GroundMove groundState;
			groundState.Update(this, mData, dt);
		} else {
			AirMove airState;
			airState.Update(this, mData, dt);
		}
	}

	// 動く床の速度を適用（接地している場合のみ）
	if (mData.isGrounded && isOnMovingSurface) {
		mData.velocity += mSurfaceVelocity;
	}

	// 衝突付き移動
	mCollisionResolver->MoveWithCollisions(dt);

	// 動く床の速度を減算（次フレームで再度加算するため）
	if (mData.isGrounded && isOnMovingSurface) {
		Vec3 horizontalSurfaceVel = mSurfaceVelocity;
		horizontalSurfaceVel.y    = 0.0f;
		mData.velocity            -= horizontalSurfaceVel;
	}

	// 空中で下方向に移動している場合、着地時の速度として保存
	if (!mData.isGrounded && mData.velocity.y < 0.0f) {
		mData.lastLandingVelocityY = mData.velocity.y;
	}

	// スタック検出と解消
	mCollisionResolver->DetectAndResolveStuck(dt);

	// 着地検知
	ProcessLanding();

	// 足音処理
	ProcessFootstep(dt);

	CheckForNaNAndClamp(); // NaNチェックと速度クランプ
}

/// @brief ジャンプ処理
void MovementComponent::ProcessJump(const bool isOnMovingSurface) {
	const bool jumpPressed = mData.wishJump && !mData.lastFrameWishJump;

	if (mData.wishJump) {
		if (mData.isGrounded) {
			// 地上ジャンプ
			if (isOnMovingSurface) {
				Vec3 horizontalSurfaceVel = mSurfaceVelocity;
				horizontalSurfaceVel.y    = 0.0f;
				mData.velocity            += horizontalSurfaceVel;
			}

			mData.velocity.y    = Math::HtoM(kJumpVelocityHu);
			mData.isGrounded    = false;
			mData.state         = MOVEMENT_STATE::AIR;
			mData.hasDoubleJump = true;

			if (mFootstepAudio) {
				mFootstepAudio->SetPitch(1.0f);
				mFootstepAudio->SetVolume(0.15f);
				mFootstepAudio->Play();
			}

			mData.jumpSnapDisableTime  = kJumpSnapDisableTime;
			mData.wasGroundedLastFrame = false;

			InputSystem::AddVibration(0, 0.3f, 0.3f, 0.1f);

			if (mData.isSliding) { EndSlide(); }
		} else if (mData.isWallRunning && !mData.wallRunJumpWasPressed) {
			// ウォールランジャンプ
			Vec3 forwardVel = mData.wallRunDirection * mData.velocity.Dot(
				                  mData.wallRunDirection
				                  );

			Vec3 awayDir = mData.wallRunNormal * 0.7f + Vec3::up * 1.0f;
			awayDir.Normalize();

			mData.velocity = forwardVel + awayDir * Math::HtoM(kWallrunJumpForce);

			if (mFootstepAudio) {
				mFootstepAudio->SetPitch(1.0f);
				mFootstepAudio->SetVolume(0.15f);
				mFootstepAudio->Play();
			}

			mData.hasDoubleJump = true;

			mData.jumpSnapDisableTime  = kJumpSnapDisableTime;
			mData.wasGroundedLastFrame = false;

			InputSystem::AddVibration(0, 0.3f, 0.3f, 0.1f);

			EndWallrun();
		} else if (!mData.isGrounded && !mData.isWallRunning &&
		           mData.hasDoubleJump && jumpPressed) {
			// ダブルジャンプ
			mData.velocity.y          = Math::HtoM(kDoubleJumpVelocityHu);
			mData.hasDoubleJump       = false;
			mData.jumpSnapDisableTime = kJumpSnapDisableTime;
			if (mFootstepAudio) {
				mFootstepAudio->SetPitch(1.2f);
				mFootstepAudio->SetVolume(0.15f);
				mFootstepAudio->Play();
			}

			InputSystem::AddVibration(0, 0.3f, 0.3f, 0.1f);
		}
	} else {
		if (mData.isWallRunning) { mData.wallRunJumpWasPressed = false; }
	}
}

/// @brief 着地検知と処理
void MovementComponent::ProcessLanding() {
	if (!mData.wasGroundedLastFrame && mData.isGrounded && !mData.isWallRunning
	    && !mData.isSliding) {
		mData.justLanded = true;
		if (mLandAudio) {
			mLandAudio->SetPitch(1.0f);
			mLandAudio->SetVolume(0.2f);
			mLandAudio->Play();
		}

		InputSystem::AddVibration(0, 0.3f, 0.2f, 0.25f);
	} else { mData.justLanded = false; }
}

/// @brief 足音処理
void MovementComponent::ProcessFootstep(const float dt) {
	if ((mData.isGrounded || mData.isWallRunning) && !mData.isSliding) {
		float speedM = 0.0f;
		if (mData.isGrounded) {
			Vec3 velHorz = mData.velocity;
			velHorz.y    = 0.0f;
			speedM       = velHorz.Length();
		} else if (mData.isWallRunning) {
			speedM = mData.velocity.Length();
		}

		if (speedM > 0.1f) {
			mStepDistance += speedM * dt;
			if (mStepDistance >= kStepIntervalM) {
				mStepDistance = 0.0f;
				if (mFootstepAudio) {
					const float randomPitch = 0.95f + (rand() % 10) * 0.01f;
					mFootstepAudio->SetPitch(randomPitch);
					mFootstepAudio->SetVolume(0.125f);
					mFootstepAudio->Play();
				}
			}
		}
	} else {
		mStepDistance = 0.0f;
	}
}

/// @brief 動く床の速度計算
void MovementComponent::UpdateMovingSurface(
	const float dt, Entity*& currentGroundEntity, bool& isOnMovingSurface
) {
	mSurfaceVelocity          = Vec3::zero;
	currentGroundEntity       = nullptr;
	isOnMovingSurface         = false;

	Unnamed::Box extendedHull = {
		.center   = mHull.center,
		.halfSize = mHull.halfSize + Vec3::one * Math::HtoM(kDynamicCheckSkinHu)
	};

	UPhysics::Hit surfaceHit;

	if (mData.isGrounded && mUPhysicsEngine->BoxOverlap(
		    extendedHull, &surfaceHit, 1
	    )) {
#ifdef _DEBUG
		DebugDraw::DrawBox(
			extendedHull.center,
			Quaternion::identity,
			extendedHull.halfSize * 2.0f,
			Vec4::purple
		);
#endif

		if (surfaceHit.hitEntity) {
			currentGroundEntity = surfaceHit.hitEntity;
			isOnMovingSurface   = true;

			auto*      transform  = currentGroundEntity->GetTransform();
			Vec3       currentPos = transform->GetWorldPos();
			Quaternion currentRot = transform->GetWorldRot();

			if (mLastGroundEntity == currentGroundEntity) {
				const Vec3 linearVelocity = (currentPos - mLastGroundPosition) / dt;

				if (dt > 0.0f) {
					Vec3 playerWorldPos = mScene->GetWorldPos();
					Vec3 localPos       = mLastGroundRotation.Inverse() * (
						                      playerWorldPos - mLastGroundPosition);
					Vec3 targetWorldPos    = currentPos + currentRot * localPos;
					Vec3 totalDisplacement = targetWorldPos - playerWorldPos;
					mSurfaceVelocity       = totalDisplacement / dt;
				}

#ifdef _DEBUG
				DebugDraw::DrawArrow(
					mScene->GetWorldPos(), mSurfaceVelocity * 0.5f, Vec4::cyan, 0.05f
				);
				if (linearVelocity.SqrLength() > 0.0001f) {
					DebugDraw::DrawArrow(
						mScene->GetWorldPos(), linearVelocity * 0.5f,
						{0.0f, 1.0f, 0.0f, 1.0f}, 0.03f
					);
				}
				DebugDraw::DrawLine(
					currentPos, mScene->GetWorldPos(), {1.0f, 1.0f, 1.0f, 0.5f}
				);
#endif
			}

			mLastGroundPosition = currentPos;
			mLastGroundRotation = currentRot;
			mLastGroundEntity   = currentGroundEntity;
		}
	} else {
		mLastGroundEntity = nullptr;
	}

	mWasOnMovingSurface = isOnMovingSurface;
}

/// @brief しゃがみ処理
void MovementComponent::UpdateCrouch(const float dt) {
	float targetHU = mData.wishCrouch ?
		                 mData.crouchHeightHu :
		                 mData.defaultHeightHu;
	if (targetHU > mData.currentHeightHu) {
		Vec3         posFeet = mScene->GetWorldPos();
		Unnamed::Box test    = {
			.center   = posFeet + Vec3::up * Math::HtoM(targetHU * 0.5f),
			.halfSize = Math::HtoM(
				{
					mData.currentWidthHu * 0.5f,
					targetHU * 0.5f,
					mData.currentWidthHu * 0.5f
				}
			)
		};
		UPhysics::Hit ov{};
		const bool    blocked = mUPhysicsEngine && mUPhysicsEngine->BoxOverlap(
			                        test, &ov
		                        );
		mData.currentHeightHu =
			blocked ?
				mData.currentHeightHu :
				std::lerp(mData.currentHeightHu, targetHU, 15.0f * dt);
	} else {
		mData.currentHeightHu = std::lerp(mData.currentHeightHu, targetHU, 15.0f * dt);
	}
}

/// @brief ハル(当たり判定)の寸法を更新
void MovementComponent::UpdateHullDimensions() {
	// 足元原点
	mHull = {
		.center = mScene->GetWorldPos() + Vec3::up * Math::HtoM(
			          mData.currentHeightHu * 0.5f
		          ),
		.halfSize = Math::HtoM(
			{
				mData.currentWidthHu * 0.5f,
				mData.currentHeightHu * 0.5f,
				mData.currentWidthHu * 0.5f
			}
		)
	};

	if (mCollider) {
		auto& [min, max] = mCollider->AABB();
		min = Vec3(-mHull.halfSize.x, 0.0f, -mHull.halfSize.z);
		max = Vec3(mHull.halfSize.x, mHull.halfSize.y * 2.0f, mHull.halfSize.z);
		auto& offset = mCollider->Offset();
		offset = Vec3::zero;
	}

	// mCollisionResolver は mHull へのポインタを保持しているため、
	// 上の代入で自動的に最新の値が反映される。追加の同期は不要。
}

/// @brief 速度と位置にNaNが含まれていないかチェックし、速度をクランプする
void MovementComponent::CheckForNaNAndClamp() {
	const float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
		GetValueAsFloat();
	for (int i = 0; i < 3; ++i) {
		if (std::isnan(mData.velocity[i])) mData.velocity[i] = 0.0f;
		if (std::isnan(mScene->GetWorldPos()[i])) {
			Vec3 pos = mScene->GetWorldPos();
			pos[i]   = 0.0f;
			mScene->SetWorldPos(pos);
		}
		mData.velocity[i] = std::min(mData.velocity[i], Math::HtoM(maxVel));
		mData.velocity[i] = std::max(mData.velocity[i], -Math::HtoM(maxVel));
	}
}

// ======================================
// ウォールラン（状態遷移ロジック）
// ======================================

/// @brief ウォールラン可能かを判定する
bool MovementComponent::CanWallrun() const {
	if (mData.wishCrouch) return false;

	const float velHorzSqr = mData.velocity.x * mData.velocity.x + mData.
	                         velocity.z * mData.velocity.z;
	const float minSpeedM = Math::HtoM(kWallrunMinSpeed);
	if (velHorzSqr < minSpeedM * minSpeedM) return false;

	if (mData.timeSinceLastWallRun < kWallrunCooldown) return false;
	if (mData.isGrounded) { return false; }
	return true;
}

/// @brief ウォールランを開始しようとする
bool MovementComponent::TryStartWallrun() {
	if (!mUPhysicsEngine) return false;

	const auto cam = CameraManager::GetActiveCamera();
	if (!cam) return false;

	Vec3 f           = cam->GetViewMat().Inverse().GetForward();
	f.y              = 0;
	const float fLen = f.Length();
	if (fLen < 1e-6f) return false;

	const Vec3 camForward = f * (1.0f / fLen);
	const Vec3 right      = Vec3::up.Cross(camForward).Normalized();

	const Vec3  checkDirections[] = {right, -right};
	const float checkDistance = Math::HtoM(mData.currentWidthHu * 0.5f + 10.0f);

	for (const Vec3& dir : checkDirections) {
		UPhysics::Hit hit{};

		if (mUPhysicsEngine->BoxCast(mHull, dir, checkDistance, &hit)) {
			Vec3 wallNormal = hit.normal.Normalized();

			if (std::abs(wallNormal.y) > 0.2f) continue;

			if (mData.timeSinceLastWallRun < kWallrunSameWallCooldown &&
			    wallNormal.Dot(mData.lastWallRunNormal) > 0.9f) { continue; }

			mData.isWallRunning = true;
			mData.wallRunNormal = wallNormal;
			mData.wallRunTime   = 0.0f;
			mData.state         = MOVEMENT_STATE::WALL_RUN;

			mData.wallRunJumpWasPressed = mData.wishJump;
			mData.hasDoubleJump         = true;

			Vec3 velHorz       = mData.velocity;
			velHorz.y          = 0;
			float currentSpeed = velHorz.Length();

			Vec3 along = Vec3::up.Cross(wallNormal).Normalized();
			if (along.Dot(camForward) < 0) { along = -along; }
			mData.wallRunDirection = along;

			float alongSpeed = velHorz.Dot(mData.wallRunDirection);
			if (std::abs(alongSpeed) > 1e-3f) {
				mData.velocity = mData.wallRunDirection * std::abs(alongSpeed);
			} else {
				mData.velocity = mData.wallRunDirection * currentSpeed;
			}

			float originalY = mData.velocity.y;
			if (originalY > 0) {
				mData.velocity.y = originalY * kWallrunVerticalDamping;
			} else if (originalY < 0) {
				mData.velocity.y = originalY * 0.3f;
			}

			float boostAmount = Math::HtoM(50.0f);
			mData.velocity    += mData.wallRunDirection * boostAmount;

			return true;
		}
	}
	return false;
}

/// @brief ウォールラン中の更新処理
void MovementComponent::UpdateWallrun(float dt) {
	mData.wallRunTime += dt;

	if (mData.wallRunTime >= kWallrunMaxTime) {
		EndWallrun();
		return;
	}

	if (mUPhysicsEngine) {
		const float   checkDistance = Math::HtoM(
			mData.currentWidthHu * 0.5f + 20.0f
		);
		UPhysics::Hit hit{};
		Vec3          toWall = -mData.wallRunNormal;
		if (!mUPhysicsEngine->BoxCast(mHull, toWall, checkDistance, &hit)) {
			EndWallrun();
			return;
		}

		Vec3  newNormal = hit.normal.Normalized();
		float normalDot = newNormal.Dot(mData.wallRunNormal);
		if (normalDot < 0.5f) {
			EndWallrun();
			return;
		}

		mData.wallRunNormal = (mData.wallRunNormal * 0.8f + newNormal * 0.2f).
			Normalized();

		if (const auto cam = CameraManager::GetActiveCamera()) {
			Vec3 camForward = cam->GetViewMat().Inverse().GetForward();
			camForward.y = 0;
			const float camForwardLen = camForward.Length();
			if (camForwardLen > 1e-6f) {
				camForward        *= 1.0f / camForwardLen;
				Vec3 projectedDir = Math::ProjectOnPlane(
					camForward, mData.wallRunNormal
				);
				const float projLen = projectedDir.Length();
				if (projLen > 1e-6f) {
					mData.wallRunDirection = projectedDir * (1.0f / projLen);
				}
			}
		}
	}

	if (mData.wishCrouch) { EndWallrun(); return; }
	if (mData.isGrounded) { EndWallrun(); return; }

	Vec3 velHorz = mData.velocity;
	velHorz.y    = 0;
	if (Math::MtoH(velHorz.Length()) < kWallrunMinSpeed * 0.5f) {
		EndWallrun();
		return;
	}

	if (kWallrunDetachOnSideInput && std::abs(mData.vecMoveInput.x) > 0.5f) {
		if (const auto cam = CameraManager::GetActiveCamera()) {
			Vec3 f           = cam->GetViewMat().Inverse().GetForward();
			f.y              = 0;
			const float fLen = f.Length();
			if (fLen > 1e-6f) {
				const Vec3  camForward = f * (1.0f / fLen);
				const Vec3  camRight   = Vec3::up.Cross(camForward).Normalized();
				const float wallSide   = camRight.Dot(mData.wallRunNormal);

				if ((wallSide > 0 && mData.vecMoveInput.x > 0.5f) ||
				    (wallSide < 0 && mData.vecMoveInput.x < -0.5f)) {
					EndWallrun();
				}
			}
		}
	}
}

/// @brief ウォールランを終了する
void MovementComponent::EndWallrun() {
	if (!mData.isWallRunning) return;

	mData.isWallRunning        = false;
	mData.state                = MOVEMENT_STATE::AIR;
	mData.lastWallRunNormal    = mData.wallRunNormal;
	mData.timeSinceLastWallRun = 0.0f;
}

// ======================================
// スライディング（状態遷移ロジック）
// ======================================

/// @brief スライド可能かを判定する
bool MovementComponent::CanSlide() const {
	if (!mData.isGrounded) return false;
	if (!mData.wishCrouch) return false;

	const float velHorzSqr = mData.velocity.x * mData.velocity.x + mData.
	                         velocity.z * mData.velocity.z;
	const float minSpeedM = Math::HtoM(kSlideMinSpeed);
	return velHorzSqr >= minSpeedM * minSpeedM;
}

/// @brief スライドを開始しようとする
void MovementComponent::TryStartSlide() {
	if (!CanSlide()) return;

	Vec3 velHorz = mData.velocity;
	velHorz.y    = 0;
	if (velHorz.IsZero()) return;

	mData.slideDirection = velHorz.Normalized();
	mData.isSliding      = true;
	mData.slideTime      = 0.0f;
	mData.state          = MOVEMENT_STATE::SLIDE;

	const float currentSpeed = velHorz.Length();
	float       boostedSpeed = currentSpeed + Math::HtoM(kSlideBoostSpeed);

	const float speedCapM = Math::HtoM(kSlideHopSpeedCap);
	boostedSpeed          = std::min(boostedSpeed, speedCapM);

	const float originalY = mData.velocity.y;
	mData.velocity        = mData.slideDirection * boostedSpeed;
	mData.velocity.y      = originalY;
}

/// @brief スライド中の更新処理
void MovementComponent::UpdateSlide(const float dt) {
	mData.slideTime += dt;

	if (mData.slideTime >= kSlideMaxTime) { EndSlide(); return; }
	if (!mData.isGrounded) { EndSlide(); return; }

	Vec3 velHoriz = mData.velocity;
	velHoriz.y    = 0;
	float speed   = Math::MtoH(velHoriz.Length());
	if (speed < kSlideStopSpeed) { EndSlide(); return; }

	if (!mData.wishCrouch) { EndSlide(); }
}

/// @brief スライドを終了する
void MovementComponent::EndSlide() {
	if (!mData.isSliding) return;

	mData.isSliding      = false;
	mData.slideDirection = Vec3::zero;
	mData.slideTime      = 0.0f;

	if (mData.isGrounded) { mData.state = MOVEMENT_STATE::GROUND; } else {
		mData.state = MOVEMENT_STATE::AIR;
	}
}
