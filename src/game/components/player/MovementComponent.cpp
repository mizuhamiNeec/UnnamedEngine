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

#include <game/components/CameraAnimator.h>

#include "KinematicCollisionResolver.h"
#include "PlayerInputController.h"
#include "states/AirMove.h"
#include "states/GroundMove.h"
#include "states/PlayerMovementStateMachine.h"
#include "states/SlideMove.h"
#include "states/WallrunMove.h"

static constexpr std::string_view kChannel = "MovementComponent";

namespace {
bool TryGetActiveCameraForward(Vec3& outForward, const bool horizontalOnly) {
	const auto cam = CameraManager::GetActiveCamera();
	if (!cam) { return false; }

	Vec3 forward = Vec3::forward;
	if (auto* owner = cam->GetOwner()) {
		if (auto* transform = owner->GetTransform()) {
			forward = transform->GetWorldRot() * Vec3::forward;
		} else { forward = cam->GetViewMat().Inverse().GetForward(); }
	} else { forward = cam->GetViewMat().Inverse().GetForward(); }

	if (horizontalOnly) { forward.y = 0.0f; }
	const float lenSq = forward.SqrLength();
	if (lenSq <= 1.0e-8f) { return false; }

	outForward = forward * (1.0f / std::sqrt(lenSq));
	return true;
}
} // namespace

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
	if (!mInputController) {
		mInputController = std::make_shared<HumanPlayerInputController>();
	}
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
			mBlinkAudio = audioMgr->GetAudio(
				"./content/parkour/sounds/se/whoosh.wav"
			);
			if (mBlinkAudio) { mBlinkAudio->SetVolume(0.3f); }
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

		// Speed Vault info
		if (mVaultActive) {
			ImGui::TextColored(
				{1.0f, 0.8f, 0.0f, 1.0f}, "SPEED VAULT! (%.2fs / %.2fs)",
				mVaultTime, kVaultDurationSec
			);
		} else if (mVaultCooldown > 0.0f) {
			ImGui::Text("Vault Cooldown: %.2fs", mVaultCooldown);
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

void MovementComponent::ApplyReplayVaultState(
	const bool isSpeedVaulting,
	const float vaultProgress,
	const Vec3& vaultStartPos,
	const Vec3& vaultApexPos,
	const Vec3& vaultEndPos
) {
	const bool wasVaultActive = mVaultActive;

	if (!isSpeedVaulting) {
		mVaultActive = false;
		return;
	}

	mVaultActive   = true;
	mVaultStartPos = vaultStartPos;
	mVaultApexPos  = vaultApexPos;
	mVaultEndPos   = vaultEndPos;
	mVaultTime = std::clamp(vaultProgress, 0.0f, 1.0f) * kVaultDurationSec;
	mData.state      = MOVEMENT_STATE::SPEED_VAULT;
	mData.isGrounded = false;

	// 通常実装のVault開始時と同様に、開始フレームだけカメラオフセットを即時反映する。
	if (!wasVaultActive && mCameraAnimator) {
		auto* animOwner = mCameraAnimator->GetOwner();
		if (animOwner && animOwner->GetParent()) {
			auto* parentTransform = animOwner->GetParent()->GetTransform();
			if (parentTransform) {
				const Vec3 landingFeetPos = mScene->GetWorldPos();
				const Vec3 headOffset     = GetHeadPos() - landingFeetPos;
				const Vec3 desiredWorldPos = mVaultStartPos + headOffset;
				const Vec3 cameraRootPos   = parentTransform->GetWorldPos();
				const Vec3 worldOffset     = desiredWorldPos - cameraRootPos;
				const Quaternion parentRot = parentTransform->GetWorldRot();
				const Vec3 localOffset = parentRot.Inverse().RotateVector(worldOffset);
				mCameraAnimator->SetVaultCameraOffset(localOffset);
				mCameraAnimator->ApplyVaultOffsetImmediate();
			}
		}
	}
}

void MovementComponent::SetInputController(
	std::shared_ptr<IPlayerInputController> controller
) {
	mInputController = std::move(controller);
	if (!mInputController) {
		mInputController = std::make_shared<HumanPlayerInputController>();
	}
}

void MovementComponent::ApplySpeedBoost(
	const float multiplier, const float durationSec
) {
	if (multiplier <= 1.0f || durationSec <= 0.0f) { return; }

	mData.speedBoostMultiplier = std::max(
		mData.speedBoostMultiplier, multiplier
	);
	mData.speedBoostTimeSec = std::max(mData.speedBoostTimeSec, durationSec);
}

bool MovementComponent::ConsumeBlinkTriggered() {
	const bool triggered = mBlinkTriggered;
	mBlinkTriggered      = false;
	return triggered;
}

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

/// @brief 壁走りの進行方向を取得する
/// @return 壁走りの進行方向ベクトル
Vec3 MovementComponent::GetWallRunDirection() const { return mData.wallRunDirection; }

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
	PlayerInputFrame frame = mInputController ?
		                         mInputController->SampleInput() :
		                         HumanPlayerInputController().SampleInput();
	mData.vecMoveInput      = frame.moveInput;

	const float sqrLen = mData.vecMoveInput.SqrLength();
	if (sqrLen > 1.0f) {
		mData.vecMoveInput       *= 1.0f / std::sqrt(sqrLen);
		mData.moveInputIntensity = 1.0f;
	} else if (sqrLen > 1e-6f) {
		mData.moveInputIntensity = std::sqrt(sqrLen);
	} else { mData.moveInputIntensity = 0.0f; }

	Vec3 wish = Vec3::zero;
	Vec3 camForward = Vec3::zero;
	if (TryGetActiveCameraForward(camForward, true)) {
		const Vec3 r = Vec3::up.Cross(camForward).Normalized();
		wish         = camForward * mData.vecMoveInput.y +
		       r * mData.vecMoveInput.x;
		wish.y       = 0.0f;
		const float wishLen = wish.Length();
		if (wishLen > 1e-6f) wish *= 1.0f / wishLen;
	}
	mData.wishDirection = wish;
	mData.wishJump      = frame.wishJump;
	mData.wishCrouch    = frame.wishCrouch;
	mData.wishBlink     = frame.wishBlink;
}

/// @brief 移動処理
/// @param dt 経過時間
void MovementComponent::ProcessMovement(const float dt) {
	// ジャンプスナップ無効時間の更新
	if (mData.jumpSnapDisableTime > 0.0f) { mData.jumpSnapDisableTime -= dt; }

	if (mBlinkCooldownSec > 0.0f) {
		mBlinkCooldownSec = std::max(0.0f, mBlinkCooldownSec - dt);
	}

	if (mVaultCooldown > 0.0f) {
		mVaultCooldown = std::max(0.0f, mVaultCooldown - dt);
	}

	// Speed Vault中は位置補間のみ行う
	if (UpdateSpeedVault(dt)) { return; }

	if (UpdateBlinkMove(dt)) { return; }

	if (mData.speedBoostTimeSec > 0.0f) {
		mData.speedBoostTimeSec = std::max(0.0f, mData.speedBoostTimeSec - dt);
		if (mData.speedBoostTimeSec == 0.0f) {
			mData.speedBoostMultiplier = 1.0f;
		}
	}

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

	HandleBlink();

	const float baseSpeed = mData.wishCrouch ?
		                        mData.crouchSpeed :
		                        mData.sprintSpeed;
	mData.currentSpeed = baseSpeed * mData.speedBoostMultiplier;

	// スライド判定
	if (mData.isGrounded && !mData.isSliding && CanSlide()) { TryStartSlide(); }
	if (mData.isSliding) { UpdateSlide(dt); }

	// ウォールラン判定
	mData.timeSinceLastWallRun += dt;

	// スピードヴォールト判定（空中で前方の低い壁を乗り越える）
	if (!mData.isGrounded && !mData.isSliding && !mData.isWallRunning
	    && CanSpeedVault()) {
		if (TryStartSpeedVault()) { return; }
	}

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

			mData.velocity = forwardVel + awayDir * Math::HtoM(
				                 kWallrunJumpForce
			                 );

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
	} else { if (mData.isWallRunning) { mData.wallRunJumpWasPressed = false; } }
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
		} else if (mData.isWallRunning) { speedM = mData.velocity.Length(); }

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
	} else { mStepDistance = 0.0f; }
}

/// @brief 動く床の速度計算
void MovementComponent::UpdateMovingSurface(
	const float dt, Entity*& currentGroundEntity, bool& isOnMovingSurface
) {
	mSurfaceVelocity    = Vec3::zero;
	currentGroundEntity = nullptr;
	isOnMovingSurface   = false;

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
				const Vec3 linearVelocity =
					(currentPos - mLastGroundPosition) / dt;

				if (dt > 0.0f) {
					Vec3 playerWorldPos = mScene->GetWorldPos();
					Vec3 localPos       = mLastGroundRotation.Inverse() * (
						                      playerWorldPos -
						                      mLastGroundPosition);
					Vec3 targetWorldPos    = currentPos + currentRot * localPos;
					Vec3 totalDisplacement = targetWorldPos - playerWorldPos;
					mSurfaceVelocity       = totalDisplacement / dt;
				}

#ifdef _DEBUG
				DebugDraw::DrawArrow(
					mScene->GetWorldPos(), mSurfaceVelocity * 0.5f, Vec4::cyan,
					0.05f
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
	} else { mLastGroundEntity = nullptr; }

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
		mData.currentHeightHu = std::lerp(
			mData.currentHeightHu, targetHU, 15.0f * dt
		);
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

	Vec3 camForward = Vec3::zero;
	if (!TryGetActiveCameraForward(camForward, true)) { return false; }
	const Vec3 right      = Vec3::up.Cross(camForward).Normalized();

	const Vec3  checkDirections[] = {right, -right};
	const float checkDistance = Math::HtoM(mData.currentWidthHu * 0.5f + 10.0f);

	for (const Vec3& dir : checkDirections) {
		UPhysics::Hit hit{};

		if (mUPhysicsEngine->BoxCast(mHull, dir, checkDistance, &hit)) {
			Vec3 wallNormal = hit.normal.Normalized();

			if (std::abs(wallNormal.y) > 0.2f) continue;

			// 速度方向が壁に対して垂直すぎたら走らない
			Vec3 velHorz = mData.velocity;
			velHorz.y    = 0;
			if (!velHorz.IsZero()) {
				Vec3  velDir = velHorz.Normalized();
				float dot    = std::abs(velDir.Dot(wallNormal));
				if (dot > kWallrunMaxNormalDot) continue;
			}

			if (mData.timeSinceLastWallRun < kWallrunSameWallCooldown &&
			    wallNormal.Dot(mData.lastWallRunNormal) > 0.9f) { continue; }

			mData.isWallRunning = true;
			mData.wallRunNormal = wallNormal;
			mData.wallRunTime   = 0.0f;
			mData.state         = MOVEMENT_STATE::WALL_RUN;

			mData.wallRunJumpWasPressed = mData.wishJump;
			mData.hasDoubleJump         = true;
			
			velHorz.y          = 0;
			float currentSpeed = velHorz.Length();

			Vec3 along = Vec3::up.Cross(wallNormal).Normalized();
			if (along.Dot(camForward) < 0) { along = -along; }
			mData.wallRunDirection = along;

			float alongSpeed = velHorz.Dot(mData.wallRunDirection);
			if (std::abs(alongSpeed) > 1e-3f) {
				mData.velocity = mData.wallRunDirection * std::abs(alongSpeed);
			} else { mData.velocity = mData.wallRunDirection * currentSpeed; }

			float originalY = mData.velocity.y;
			if (originalY > 0) {
				mData.velocity.y = originalY * kWallrunVerticalDamping;
			} else if (originalY < 0) { mData.velocity.y = originalY * 0.3f; }

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
		const float checkDistance = Math::HtoM(
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

		Vec3 camForward = Vec3::zero;
		if (TryGetActiveCameraForward(camForward, true)) {
			Vec3 projectedDir = Math::ProjectOnPlane(
				camForward, mData.wallRunNormal
			);
			const float projLen = projectedDir.Length();
			if (projLen > 1e-6f) {
				mData.wallRunDirection = projectedDir * (1.0f / projLen);
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
		Vec3 camForward = Vec3::zero;
		if (TryGetActiveCameraForward(camForward, true)) {
			const Vec3  camRight = Vec3::up.Cross(camForward).Normalized();
			const float wallSide = camRight.Dot(mData.wallRunNormal);

			if ((wallSide > 0 && mData.vecMoveInput.x > 0.5f) ||
			    (wallSide < 0 && mData.vecMoveInput.x < -0.5f)) {
				EndWallrun();
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

	if (mData.slideTime >= kSlideMaxTime) {
		EndSlide();
		return;
	}
	if (!mData.isGrounded) {
		EndSlide();
		return;
	}

	// 斜面に沿った重力の影響を適用
	// 地面の法線を使い、重力ベクトルを斜面に投影して加速/減速する
	{
		const Vec3 groundNormal = mData.lastGroundNormal;
		// 地面が水平でない場合のみ斜面重力を適用
		if (groundNormal.y < 0.999f && groundNormal.y > 0.0f) {
			const float gravity = Math::HtoM(
				ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat()
			);

			// 重力ベクトルを斜面に投影
			// slopeGravity = gravity - (gravity · normal) * normal
			const Vec3 gravityVec  = Vec3::down * gravity;
			const float dotGN      = gravityVec.Dot(groundNormal);
			const Vec3 slopeForce  = (gravityVec - groundNormal * dotGN)
			                       * kSlideGravityScale;

			mData.velocity += slopeForce * dt;
		}
	}

	Vec3 velHoriz = mData.velocity;
	velHoriz.y    = 0;
	float speed   = Math::MtoH(velHoriz.Length());
	if (speed < kSlideStopSpeed) {
		EndSlide();
		return;
	}

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

bool MovementComponent::UpdateBlinkMove(const float dt) {
	if (!mBlinkMoveActive) { return false; }

	mBlinkMoveTime += dt;
	const float t  = std::clamp(
		mBlinkMoveTime / kBlinkMoveDurationSec, 0.0f, 1.0f
	);
	const Vec3 newPos = Math::Lerp(mBlinkStartPos, mBlinkTargetPos, t);
	mScene->SetWorldPos(newPos);
	UpdateHullDimensions();

	if (t >= 1.0f) { mBlinkMoveActive = false; }

	return true;
}

/// @brief ブリンク処理
void MovementComponent::HandleBlink() {
	if (!mData.wishBlink || mBlinkCooldownSec > 0.0f || mBlinkMoveActive) {
		return;
	}

	const auto camera = CameraManager::GetActiveCamera();
	if (!camera) { return; }

	Vec3 dir = Vec3::zero;
	if (!TryGetActiveCameraForward(dir, false)) { return; }

	const Vec3 startPos = mScene->GetWorldPos();
	const Vec3 desiredDisplacement = dir * Math::HtoM(kBlinkDistanceHu);
	Vec3 resolvedPos = startPos + desiredDisplacement;

	if (mCollisionResolver) {
		if (!mCollisionResolver->CollideAndSlide(
			startPos,
			desiredDisplacement,
			resolvedPos
		)) {
			return;
		}
	}

	const Vec3 displacement = resolvedPos - startPos;
	if (displacement.SqrLength() <= 1e-8f) { return; }

	const float verticalSpeed   = mData.velocity.y;
	Vec3        horizontalVel   = mData.velocity;
	horizontalVel.y             = 0.0f;
	const float horizontalSpeed = horizontalVel.Length();

	Vec3  horizontalDir   = displacement;
	horizontalDir.y       = 0.0f;
	const float horizLenSq = horizontalDir.SqrLength();
	if (horizLenSq > 1e-6f && horizontalSpeed > 0.0f) {
		horizontalDir *= 1.0f / std::sqrt(horizLenSq);
		mData.velocity = horizontalDir * horizontalSpeed;
		mData.velocity.y = verticalSpeed;
	}

	mBlinkStartPos   = startPos;
	mBlinkTargetPos  = resolvedPos;
	mBlinkMoveTime   = 0.0f;
	mBlinkMoveActive = true;

	UpdateHullDimensions();

	mData.isGrounded          = false;
	mData.state               = MOVEMENT_STATE::AIR;
	mData.jumpSnapDisableTime = kJumpSnapDisableTime;

	mBlinkCooldownSec = kBlinkCooldownSec;
	mBlinkTriggered   = true;
	if (mBlinkAudio) { mBlinkAudio->Play(); }
}

// ======================================
// スピードヴォールト（壁乗り越え）
// ======================================

/// @brief スピードヴォールト可能かを判定する
bool MovementComponent::CanSpeedVault() const {
	if (mData.isGrounded) return false;  // 空中でのみ発動
	if (mData.wishCrouch) return false;
	if (mVaultCooldown > 0.0f) return false;
	if (mVaultActive) return false;

	// 前方入力チェック（前に向かって進もうとしているとき限定）
	if (mData.vecMoveInput.y < 0.5f) return false;

	return true;
}

/// @brief スピードヴォールトを開始しようとする
/// @return 開始成功ならtrue
bool MovementComponent::TryStartSpeedVault() {
	if (!mUPhysicsEngine) return false;

	// カメラの前方方向を取得（水平）
	const auto cam = CameraManager::GetActiveCamera();
	if (!cam) return false;

	Vec3 forward = Vec3::zero;
	if (!TryGetActiveCameraForward(forward, true)) { return false; }

	const Vec3  feetPos       = mScene->GetWorldPos();
	const float halfWidthM    = Math::HtoM(mData.currentWidthHu * 0.5f);
	const float playerHeightM = Math::HtoM(mData.currentHeightHu);
	const float checkDistM    = Math::HtoM(kVaultForwardCheckHu);
	const float maxVaultHeightM = Math::HtoM(kVaultMaxHeightHu);

	// 1) 前方にBoxCastして壁を検出（空中なので足元〜腰付近）
	Unnamed::Box forwardProbe = {
		.center   = feetPos + Vec3::up * (playerHeightM * 0.25f),
		.halfSize = { halfWidthM, playerHeightM * 0.25f, halfWidthM }
	};

	UPhysics::Hit wallHit{};
	float distToWall = 0.0f;
	bool  wallFound  = false;

	// まずBoxCastで前方に壁があるか確認
	if (mUPhysicsEngine->BoxCast(
		forwardProbe, forward, checkDistM + halfWidthM, &wallHit
	)) {
		Vec3 wallNormal = wallHit.normal.Normalized();
		if (std::abs(wallNormal.y) > 0.3f) return false; // 斜面は対象外
		distToWall = std::max(0.0f, wallHit.t);
		wallFound  = true;
	}

	// BoxCastで見つからなかった場合、すでに壁に密着している可能性がある
	// → Overlapで確認
	if (!wallFound) {
		UPhysics::Hit overlapHit{};
		if (mUPhysicsEngine->BoxOverlap(forwardProbe, &overlapHit)) {
			Vec3 wallNormal = overlapHit.normal.Normalized();
			if (std::abs(wallNormal.y) > 0.3f) return false;
			distToWall = 0.0f;
			wallFound  = true;
		}
	}

	if (!wallFound) return false; // 前方に壁がない

	// 2) 壁の上端を特定するため、上方向にプローブを移動させながらチェック
	//    ステップ的に上に移動し、前方に壁がなくなった高さを特定
	float wallTopHeightM = 0.0f;
	bool  foundTop = false;

	// 小さなプローブで段階的にチェック
	const float probeStepM  = Math::HtoM(8.0f);   // 8HUステップ
	const float probeSizeM  = Math::HtoM(4.0f);    // 小さなプローブ

	// 空中なので足元付近からチェック開始（足元より少し下も含む）
	const float startCheckM = -Math::HtoM(16.0f);

	for (float testHeight = startCheckM;
	     testHeight <= maxVaultHeightM + probeStepM;
	     testHeight += probeStepM) {
		Unnamed::Box topProbe = {
			.center   = feetPos + Vec3::up * testHeight,
			.halfSize = { probeSizeM, probeSizeM, probeSizeM }
		};

		UPhysics::Hit topHit{};
		if (!mUPhysicsEngine->BoxCast(
			topProbe, forward, checkDistM + halfWidthM * 2.0f, &topHit
		)) {
			// この高さでは前方に障害物なし → 壁の上端はこの高さ以下
			wallTopHeightM = testHeight;
			foundTop = true;
			break;
		}
	}

	if (!foundTop) return false; // 壁が高すぎる

	// 壁の上端がプレイヤーの最大ヴォールト高さを超えていないか確認
	if (wallTopHeightM > maxVaultHeightM) return false;

	// 壁の上面が概ね上方向を向いているか確認
	// 壁上端の少し上から下方向にBoxCastして上面の法線を取得
	{
		Vec3 surfaceCheckPos = feetPos + forward * (distToWall + halfWidthM)
		                     + Vec3::up * (wallTopHeightM + Math::HtoM(8.0f));
		Unnamed::Box surfaceProbe = {
			.center   = surfaceCheckPos,
			.halfSize = { probeSizeM, probeSizeM, probeSizeM }
		};
		UPhysics::Hit surfaceHit{};
		if (mUPhysicsEngine->BoxCast(
			surfaceProbe, Vec3::down, Math::HtoM(16.0f), &surfaceHit
		)) {
			// 上面の法線が歩行可能な角度でなければ中止
			if (surfaceHit.normal.y < mData.groundNormalY) return false;
		}
	}

	// 3) 壁の向こう側に着地スペースがあるか確認
	// 壁の上端の少し上で前方にBoxCastし、壁の厚さを測定する
	float wallThicknessM = 0.0f;
	{
		Vec3 thicknessCheckPos = feetPos + Vec3::up * (wallTopHeightM - probeStepM);
		Unnamed::Box thicknessProbe = {
			.center   = thicknessCheckPos,
			.halfSize = { probeSizeM, probeSizeM, probeSizeM }
		};
		UPhysics::Hit thicknessHit{};
		float thicknessCheckDist = Math::HtoM(256.0f);
		if (mUPhysicsEngine->BoxCast(
			thicknessProbe, forward, thicknessCheckDist, &thicknessHit
		)) {
			// 壁の裏面にヒット → distToWall からこのヒット距離までが壁の厚さ
			wallThicknessM = thicknessHit.t + probeSizeM * 2.0f;
		} else {
			// 裏面が見つからない → 壁の厚さ不明、安全側で推定
			wallThicknessM = halfWidthM * 2.0f;
		}
	}

	// 壁の向こう側のオフセット = 壁面まで + 壁の厚さ + プレイヤー半幅 + マージン
	const float overWallOffsetM = std::max(
		distToWall + wallThicknessM + halfWidthM + Math::HtoM(8.0f),
		halfWidthM * 3.0f + Math::HtoM(8.0f)
	);

	// 壁の向こう側の位置（壁の上端の高さ）
	Vec3 overWallPos = feetPos + forward * overWallOffsetM
	                 + Vec3::up * (wallTopHeightM + Math::HtoM(4.0f));

	// 向こう側から下方向にレイキャストして着地点を探す
	Unnamed::Box landingProbe = {
		.center   = overWallPos,
		.halfSize = { halfWidthM, Math::HtoM(2.0f), halfWidthM }
	};

	UPhysics::Hit landHit{};
	const float   dropCheckDistM = maxVaultHeightM + Math::HtoM(32.0f);
	if (!mUPhysicsEngine->BoxCast(
		landingProbe, Vec3::down, dropCheckDistM, &landHit
	)) {
		return false; // 着地点が見つからない → 崖の可能性、中止
	}

	// 着地点の法線が歩行可能か確認
	if (landHit.normal.y < mData.groundNormalY) return false;

	// 着地位置（足元）
	Vec3 landingFeetPos = overWallPos + Vec3::down * landHit.t;

	// 着地位置が開始位置より下にならないよう制限（上方向のみのVault）
	if (landingFeetPos.y < feetPos.y) {
		landingFeetPos.y = feetPos.y;
	}

	// 着地位置でプレイヤーのハルが重なりなしか確認
	Unnamed::Box landingHull = mCollisionResolver->BuildHullAtFeet(landingFeetPos);
	UPhysics::Hit overlapHit{};
	if (mUPhysicsEngine->BoxOverlap(landingHull, &overlapHit)) {
		return false; // 着地位置にスペースがない
	}

	// 着地位置から壁側に向かってBoxCastし、壁の裏面でないことを確認
	// 壁の表面の法線が自分の方を向いていなければ裏側にいる
	{
		Unnamed::Box backCheckProbe = {
			.center   = landingFeetPos + Vec3::up * (playerHeightM * 0.5f),
			.halfSize = { probeSizeM, playerHeightM * 0.5f, probeSizeM }
		};
		UPhysics::Hit backHit{};
		if (mUPhysicsEngine->BoxCast(
			backCheckProbe, -forward, overWallOffsetM, &backHit
		)) {
			// 壁にヒットした場合、法線がforwardと同じ方向(=裏側の面)なら中止
			float dot = backHit.normal.Dot(forward);
			if (dot > 0.3f) {
				return false; // 壁の裏面が見えている → 裏側にテレポートしてしまう
			}
		}
	}

	// 4) ヴォールト開始！
	const float apexForwardM = std::max(halfWidthM, distToWall * 0.5f);
	Vec3 apexPos = feetPos + forward * apexForwardM
	             + Vec3::up * (wallTopHeightM + Math::HtoM(8.0f));

	mVaultStartPos    = feetPos;
	mVaultApexPos     = apexPos;
	mVaultEndPos      = landingFeetPos;
	mVaultPreVelocity = mData.velocity;
	mVaultTime        = 0.0f;
	mVaultActive      = true;

	// ハルを即座に着地位置にテレポート（めり込み防止）
	mScene->SetWorldPos(landingFeetPos);
	UpdateHullDimensions();

	// テレポート直後にカメラオフセットを即時適用（1フレームのカメラジャンプ防止）
	// cameraRootはまだテレポート前のHeadPosにいるため、
	// ベジェt=0(=feetPos)のカメラ位置を保つためのオフセットを設定
	if (mCameraAnimator) {
		auto* animOwner = mCameraAnimator->GetOwner();
		if (animOwner && animOwner->GetParent()) {
			auto* parentTransform = animOwner->GetParent()->GetTransform();
			if (parentTransform) {
				Vec3 headOffset      = GetHeadPos() - landingFeetPos;
				Vec3 desiredWorldPos = feetPos + headOffset;
				Vec3 cameraRootPos   = parentTransform->GetWorldPos();
				Vec3 worldOffset     = desiredWorldPos - cameraRootPos;
				Quaternion parentRot = parentTransform->GetWorldRot();
				Vec3 localOffset     = parentRot.Inverse().RotateVector(worldOffset);

				mCameraAnimator->SetVaultCameraOffset(localOffset);
				mCameraAnimator->ApplyVaultOffsetImmediate();
			}
		}
	}

	mData.isGrounded          = false;
	mData.state               = MOVEMENT_STATE::SPEED_VAULT;
	mData.jumpSnapDisableTime = kJumpSnapDisableTime;

	if (mFootstepAudio) {
		mFootstepAudio->SetPitch(1.1f);
		mFootstepAudio->SetVolume(0.15f);
		mFootstepAudio->Play();
	}

	InputSystem::AddVibration(0, 0.2f, 0.2f, 0.15f);

#ifdef _DEBUG
	DebugDraw::DrawBox(
		landingHull.center, Quaternion::identity,
		landingHull.halfSize * 2.0f, Vec4::green
	);
#endif

	return true;
}

/// @brief スピードヴォールト中の位置補間更新
/// @param dt 経過時間
/// @return ヴォールト中ならtrue
bool MovementComponent::UpdateSpeedVault(const float dt) {
	if (!mVaultActive) return false;

	mVaultTime += dt;
	const float t = std::clamp(mVaultTime / kVaultDurationSec, 0.0f, 1.0f);

#ifdef _DEBUG
	DebugDraw::DrawLine(mVaultStartPos, mVaultApexPos, Vec4::yellow);
	DebugDraw::DrawLine(mVaultApexPos, mVaultEndPos, Vec4::yellow);
#endif

	if (t >= 1.0f) {
		EndSpeedVault();
	}

	return true;
}

/// @brief スピードヴォールト終了処理
void MovementComponent::EndSpeedVault() {
	mVaultActive = false;

	// 水平速度を維持（垂直速度は0にリセット）
	Vec3 horizontalVel = mVaultPreVelocity;
	horizontalVel.y    = 0.0f;
	float horizontalSpeed = horizontalVel.Length();

	// 着地方向に速度を向ける
	Vec3 vaultDir = mVaultEndPos - mVaultStartPos;
	vaultDir.y    = 0.0f;
	const float vaultDirLen = vaultDir.Length();

	// 速度がほぼ0だった場合（壁密着からのヴォールト等）、最低速度を付与
	const float minExitSpeedM = Math::HtoM(kVaultMinSpeedHu);
	if (horizontalSpeed < minExitSpeedM) {
		horizontalSpeed = minExitSpeedM;
	}

	if (vaultDirLen > 1e-6f) {
		vaultDir *= 1.0f / vaultDirLen;
		mData.velocity   = vaultDir * horizontalSpeed;
		mData.velocity.y = 0.0f;
	} else {
		mData.velocity   = mData.wishDirection * horizontalSpeed;
		mData.velocity.y = 0.0f;
	}

	mData.isGrounded    = true;
	mData.state         = MOVEMENT_STATE::GROUND;
	mData.hasDoubleJump = true;

	mVaultCooldown = kVaultCooldownSec;
}

/// @brief スピードヴォールト中かを取得する
/// @return スピードヴォールト中ならtrue
bool MovementComponent::IsSpeedVaulting() const { return mVaultActive; }

/// @brief スピードヴォールトの進行度を取得する
/// @return 進行度 (0.0〜1.0)、Vault中でなければ1.0
float MovementComponent::GetVaultProgress() const {
	if (!mVaultActive) return 1.0f;
	return std::clamp(mVaultTime / kVaultDurationSec, 0.0f, 1.0f);
}

