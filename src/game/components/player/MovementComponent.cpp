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
}

/// @brief 物理演算前の更新
/// @param deltaTime 経過時間
void MovementComponent::PrePhysics(float) {
	ProcessInput();

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
}

/// @brief 更新
/// @param dt 経過時間
void MovementComponent::Update(const float dt) { ProcessMovement(dt); }

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
		mData.vecMoveInput *= 1.0f / std::sqrt(sqrLen);
		mData.moveInputIntensity = 1.0f;
	} else if (sqrLen > 1e-6f) {
		mData.moveInputIntensity = std::sqrt(sqrLen);
	} else {
		mData.moveInputIntensity = 0.0f;
	}

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

	// 動く床の速度計算と適用
	mSurfaceVelocity            = Vec3::zero;
	Entity* currentGroundEntity = nullptr;

	// プレイヤーの足元周辺で床を検出（接地している場合のみ）
	Unnamed::Box extendedHull = {
		.center   = mHull.center,
		.halfSize = mHull.halfSize + Vec3::one * Math::HtoM(kDynamicCheckSkinHu)
	};

	UPhysics::Hit surfaceHit;
	bool          isOnMovingSurface = false;

	// 接地している場合のみ床を検出
	if (mData.isGrounded && mUPhysicsEngine->BoxOverlap(
		    extendedHull, &surfaceHit, 1
	    )) {
		DebugDraw::DrawBox(
			extendedHull.center,
			Quaternion::identity,
			extendedHull.halfSize * 2.0f,
			Vec4::purple
		);

		// 接触したオブジェクトの速度を計算
		if (surfaceHit.hitEntity) {
			currentGroundEntity = surfaceHit.hitEntity;
			isOnMovingSurface   = true;

			auto*      transform  = currentGroundEntity->GetTransform();
			Vec3       currentPos = transform->GetWorldPos();
			Quaternion currentRot = transform->GetWorldRot();

			// 同じエンティティに接触し続けている場合のみ速度を計算
			if (mLastGroundEntity == currentGroundEntity) {
				// 移動による速度
				const Vec3 linearVelocity = (currentPos - mLastGroundPosition) /
				                            dt;

				// 回転による速度（プレイヤー位置での接線速度）
				if (dt > 0.0f) {
					Vec3 playerWorldPos = mScene->GetWorldPos();

					Vec3 localPos = mLastGroundRotation.Inverse() * (
						                playerWorldPos - mLastGroundPosition);

					Vec3 targetWorldPos = currentPos + currentRot * localPos;

					Vec3 totalDisplacement = targetWorldPos - playerWorldPos;
					mSurfaceVelocity       = totalDisplacement / dt;
				}
				// デバッグ表示
				// 合成速度（シアン）
				DebugDraw::DrawArrow(
					mScene->GetWorldPos(),
					mSurfaceVelocity * 0.5f,
					Vec4::cyan,
					0.05f
				);

				// 線形速度（緑）
				if (linearVelocity.SqrLength() > 0.0001f) {
					DebugDraw::DrawArrow(
						mScene->GetWorldPos(),
						linearVelocity * 0.5f,
						{0.0f, 1.0f, 0.0f, 1.0f}, // 緑
						0.03f
					);
				}

				// 回転中心からプレイヤーへの半径ベクトル（白）
				DebugDraw::DrawLine(
					currentPos,
					mScene->GetWorldPos(),
					{1.0f, 1.0f, 1.0f, 0.5f} // 白（半透明）
				);
			}

			// 現在の状態を保存
			mLastGroundPosition = currentPos;
			mLastGroundRotation = currentRot;
			mLastGroundEntity   = currentGroundEntity;
		}
	} else {
		// 空中にいる場合はトラッキングをリセット
		mLastGroundEntity = nullptr;
	}

	// 床から離れた場合の処理は後で行う（ジャンプ処理の後）
	mWasOnMovingSurface = isOnMovingSurface;

	// 前フレームの接地状態を記録
	mData.wasGroundedLastFrame = mData.isGrounded;

	// しゃがんでいるかで高さを決定
	float targetHU = mData.wishCrouch ?
		                 mData.crouchHeightHu :
		                 mData.defaultHeightHu;
	if (targetHU > mData.currentHeightHu) {
		// 立てるかチェック
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
		// しゃがみ
		mData.currentHeightHu =
			std::lerp(mData.currentHeightHu, targetHU, 15.0f * dt);
	}

	// ヒットボックス更新
	UpdateHullDimensions();

	mData.currentSpeed = mData.wishCrouch ?
		                     mData.crouchSpeed :
		                     mData.sprintSpeed;

	if (mData.isGrounded && !mData.isSliding && CanSlide()) { TryStartSlide(); }

	// スライディング更新
	if (mData.isSliding) { UpdateSlide(dt); }

	const float wishspeed = mData.wishDirection.IsZero() ?
		                        0.0f :
		                        mData.currentSpeed * mData.moveInputIntensity;

	mData.timeSinceLastWallRun += dt;
	if (!mData.isGrounded && !mData.isWallRunning && CanWallrun()) {
		TryStartWallrun();
	}

	// ジャンプキーが離されてから押された検出（バニーホップ対策）
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
			// Wallrun jump: キーを一度離してから再度押した場合のみ
			// (バニーホップでの誤発動防止)
			// 壁方向入力は完全に無視（常にジャンプ可能）

			// 現在の進行方向の速度を取得
			Vec3 forwardVel = mData.wallRunDirection * mData.velocity.Dot(
				                  mData.wallRunDirection
			                  );

			// 壁から離れる方向（横と上）
			Vec3 awayDir = mData.wallRunNormal * 0.7f + Vec3::up * 1.0f;
			awayDir.Normalize();

			// 進行方向の速度 + 壁から離れるブースト
			mData.velocity = forwardVel + awayDir * Math::HtoM(
				                 kWallrunJumpForce
			                 );

			if (mFootstepAudio) {
				mFootstepAudio->SetPitch(1.0f);
				mFootstepAudio->SetVolume(0.15f);
				mFootstepAudio->Play();
			}

			mData.hasDoubleJump = true; // ウォールランジャンプでダブルジャンプリセット

			mData.jumpSnapDisableTime  = kJumpSnapDisableTime;
			mData.wasGroundedLastFrame = false; // 念のため追加

			InputSystem::AddVibration(0, 0.3f, 0.3f, 0.1f);

			EndWallrun();
		} else if (!mData.isGrounded && !mData.isWallRunning && mData.
		           hasDoubleJump && jumpPressed) {
			// ダブルジャンプ（空中で、キーを離してから押した場合）
			mData.velocity.y          = Math::HtoM(kDoubleJumpVelocityHu);
			mData.hasDoubleJump       = false; // 使用済み
			mData.jumpSnapDisableTime = kJumpSnapDisableTime;
			if (mFootstepAudio) {
				mFootstepAudio->SetPitch(1.2f);
				mFootstepAudio->SetVolume(0.15f);
				mFootstepAudio->Play();
			}

			InputSystem::AddVibration(0, 0.3f, 0.3f, 0.1f);
		}
	} else {
		// ジャンプキーが離されたらフラグをリセット
		if (mData.isWallRunning) { mData.wallRunJumpWasPressed = false; }
	}

	// 前フレームのジャンプ入力を保存
	mData.lastFrameWishJump = mData.wishJump;

	const float gravityValue = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();

	if (mData.isWallRunning) {
		// Wallrun gravity (reduced)
		mData.velocity.y -= Math::HtoM(gravityValue) * kWallrunGravityScale *
			dt;
		Wallrun(wishspeed, dt);
		UpdateWallrun(dt);
	} else if (mData.isSliding) {
		// Slide physics
		if (!mData.isGrounded) ApplyHalfGravity(dt);
		Slide(wishspeed, dt);
		if (!mData.isGrounded) ApplyHalfGravity(dt);
	} else {
		if (!mData.isGrounded) ApplyHalfGravity(dt);
		if (mData.isGrounded) Ground(wishspeed, dt);
		else Air(wishspeed, dt);
		if (!mData.isGrounded) ApplyHalfGravity(dt);
	}

	// 動く床の速度を適用（接地している場合のみ）
	// 一時的に速度を加算して移動し、その後減算して相対速度を維持
	if (mData.isGrounded && isOnMovingSurface) {
		mData.velocity += mSurfaceVelocity;
	}

	// 衝突付き移動
	MoveWithCollisions(dt);

	// 動く床の速度を減算（次フレームで再度加算するため）
	// ただし、空中に離れた場合は減算しない（慣性を保持）
	if (mData.isGrounded && isOnMovingSurface) {
		// まだ接地している場合は速度を元に戻す
		Vec3 horizontalSurfaceVel = mSurfaceVelocity;
		horizontalSurfaceVel.y    = 0.0f;
		mData.velocity            -= horizontalSurfaceVel;

		// Y方向は衝突で変化している可能性があるので保持
	} else if (!mData.isGrounded && isOnMovingSurface) {
		// 移動中に空中に離れた場合、床の速度は既に継承されているので何もしない
	}

	// 空中で下方向に移動している場合、着地時の速度として保存
	if (!mData.isGrounded && mData.velocity.y < 0.0f) {
		mData.lastLandingVelocityY = mData.velocity.y;
	}

	// スタック検出と解消
	DetectAndResolveStuck(dt);

	// 前フレームで空中、今フレームで地上 = 着地
	if (!mData.wasGroundedLastFrame && mData.isGrounded && !mData.isWallRunning
	    && !mData.isSliding) {
		mData.justLanded = true;
		if (mLandAudio) {
			mLandAudio->SetPitch(1.0f);
			mLandAudio->SetVolume(0.2f);
			mLandAudio->Play();
		}

		InputSystem::AddVibration(
			0, 0.3f, 0.2f, 0.25f
		);
	} else { mData.justLanded = false; }

	// 足音処理
	if ((mData.isGrounded || mData.isWallRunning) && !mData.isSliding) {
		float speedM = 0.0f;
		if (mData.isGrounded) {
			Vec3 velHorz = mData.velocity;
			velHorz.y    = 0.0f;
			speedM       = velHorz.Length();
		} else if (mData.isWallRunning) {
			// ウォールラン中は全速度を考慮
			speedM = mData.velocity.Length();
		}

		if (speedM > 0.1f) {
			mStepDistance += speedM * dt;
			if (mStepDistance >= kStepIntervalM) {
				mStepDistance = 0.0f;
				if (mFootstepAudio) {
					// ピッチ変化
					const float randomPitch = 0.95f + (rand() % 10) * 0.01f;
					mFootstepAudio->SetPitch(randomPitch);
					mFootstepAudio->SetVolume(0.125f);
					mFootstepAudio->Play();
				}
			}
		}
	} else {
		// 空中などは距離リセット
		mStepDistance = 0.0f;
	}

	CheckForNaNAndClamp(); // NaNチェックと速度クランプ
}

/// @brief 地上での移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Ground(const float wishspeed, const float dt) {
	// Y成分は0にして水平移動のみ計算
	mData.velocity.y = 0.0f;

	const float groundFriction = ConVarManager::GetConVar("sv_friction")->
		GetValueAsFloat();

	Friction(groundFriction, dt); // 摩擦を適用

	Vec3 wishdir = mData.wishDirection;
	wishdir.y    = 0.0f; // 水平方向のみ

	const float wishdirSqrLen = wishdir.SqrLength();
	if (wishdirSqrLen > 1e-8f && wishspeed > 0.0f) {
		wishdir *= 1.0f / std::sqrt(wishdirSqrLen);
		// 加速
		Accelerate(
			wishdir,
			wishspeed,
			ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat(),
			dt
		);
	}
}

/// @brief 空中での移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Air(const float wishspeed, const float dt) {
	Vec3 wishdir = mData.wishDirection;
	wishdir.y    = 0.0f;
	AirAccelerate(
		wishdir,
		wishspeed,
		ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat(),
		dt
	);
}

/// @brief 壁走り中の移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::ApplyHalfGravity(float dt) {
	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	mData.velocity.y -= Math::HtoM(g) * 0.5f * dt;
}

/// @brief 摩擦の適用
/// @param amount 摩擦量
/// @param dt 経過時間
void MovementComponent::Friction(const float amount, const float dt) {
	if (!mData.isGrounded) return;

	// Quake/Source: 水平速度のみで摩擦を計算
	Vec3 vel_horz      = mData.velocity;
	vel_horz.y         = 0;
	const float speedM = vel_horz.Length();
	const float speed  = Math::MtoH(speedM);
	if (speed < 0.1f) return;

	const float stop = ConVarManager::GetConVar("sv_stopspeed")->
		GetValueAsFloat();
	const float ctrl = speed < stop ? stop : speed;

	const float drop = ctrl * amount * dt;

	float newspeed = std::max(0.0f, speed - drop);

	if (newspeed != speed) {
		newspeed       /= speed;
		mData.velocity *= newspeed;
	}
}

/// @brief 加速の適用
/// @param dir 加速方向（正規化済み）
/// @param speed 目標速度
/// @param accel 加速度係数
/// @param dt 経過時間
void MovementComponent::Accelerate(
	const Vec3  dir, const float   speed,
	const float accel, const float dt
) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float cur = Math::MtoH(mData.velocity).Dot(dir);
	const float add = speed - cur;
	if (add <= 0.f) return;
	float acc      = std::min(accel * speed * dt, add);
	mData.velocity += Math::HtoM(acc) * dir;
}

/// @brief 加速の適用（空中用）
/// @param dir 加速方向（正規化済み）
/// @param speed 目標速度
/// @param accel 加速度係数
/// @param dt 経過時間
void MovementComponent::AirAccelerate(
	const Vec3  dir, const float   speed,
	const float accel, const float dt
) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float wishspd = std::min(speed, kAirSpeedCap);
	const float cur     = Math::MtoH(mData.velocity).Dot(dir);
	const float add     = wishspd - cur;
	if (add <= 0.f) return;
	const float acc = std::min(accel * speed * dt, add);
	mData.velocity  += Math::HtoM(acc) * dir;
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

	// ハルをもとにコライダーを更新
	// AABBはローカル座標で保持し、オフセットで調整する
	if (mCollider) {
		auto& [min, max] = mCollider->AABB();
		// ローカル座標でのAABB（足元を原点とする）
		min = Vec3(-mHull.halfSize.x, 0.0f, -mHull.halfSize.z);
		max = Vec3(mHull.halfSize.x, mHull.halfSize.y * 2.0f, mHull.halfSize.z);
		auto& offset = mCollider->Offset();
		// オフセットは不要（足元基準なので）
		offset = Vec3::zero;
	}
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

void MovementComponent::ResolvePenetration() {
	if (!mUPhysicsEngine || !mCollider) return;

	// 最大反復回数（コーナーなどで複数の壁に挟まれた場合用）
	constexpr int kMaxIterations = 4;

	for (int iter = 0; iter < kMaxIterations; ++iter) {
		// 現在のハルで重なりをチェック
		// ※重なっている全てのオブジェクトを取得したいが、
		//   BoxOverlapが単一ヒットか複数ヒットか不明なため、とりあえず単一ヒットでループ処理して解消する
		UPhysics::Hit hit{};

		// 自分のハルを少し小さくして判定するか、そのまま判定するか
		// ここでは正確なAABB判定を行うため、現在のハルを使用
		if (!mUPhysicsEngine->BoxOverlap(mHull, &hit)) {
			break; // 重なりなし
		}

		// 相手がトリガーや、接触判定のないエンティティなら無視
		if (!hit.hitEntity) break;
		// ※必要に応じて、相手が「動く床/壁」タグを持っているかチェックしても良い

		// 相手のAABBを取得
		auto* otherCollider = hit.hitEntity->GetComponent<AABBCollider>();
		if (!otherCollider) break;

		// ワールド座標系でのAABBを取得
		Vec3 otherMin, otherMax;
		{
			auto [localMin, localMax] = otherCollider->AABB();
			Vec3 offset = otherCollider->Offset();
			Vec3 otherPos = hit.hitEntity->GetTransform()->GetWorldPos();
			otherMin = otherPos + offset + localMin;
			otherMax = otherPos + offset + localMax;
		}

		// 自分のAABB（現在のハルから計算）
		Vec3 myMin = mHull.center - mHull.halfSize;
		Vec3 myMax = mHull.center + mHull.halfSize;

		// 各軸の重なり量を計算
		float overlapX = std::min(myMax.x, otherMax.x) - std::max(
			                 myMin.x, otherMin.x
		                 );
		float overlapY = std::min(myMax.y, otherMax.y) - std::max(
			                 myMin.y, otherMin.y
		                 );
		float overlapZ = std::min(myMax.z, otherMax.z) - std::max(
			                 myMin.z, otherMin.z
		                 );

		// 重なっていない軸があれば終了（念のため）
		if (overlapX <= 0 || overlapY <= 0 || overlapZ <= 0) break;

		// 最も浅い（脱出しやすい）軸を探す
		// Y軸（上下）は、ステップアップなどで誤検知しやすいので、少し優先度を下げるか慎重に扱う
		// ここでは純粋に最小の重なりを採用
		float minOverlap = overlapX;
		auto  pushDir    = Vec3(1, 0, 0); // X軸

		if (overlapY < minOverlap) {
			minOverlap = overlapY;
			pushDir    = Vec3(0, 1, 0); // Y軸
		}
		if (overlapZ < minOverlap) {
			minOverlap = overlapZ;
			pushDir    = Vec3(0, 0, 1); // Z軸
		}

		// 押し出す方向を決定（相手の中心から自分の中心への方向）
		Vec3 otherCenter = (otherMin + otherMax) * 0.5f;
		Vec3 myCenter    = mHull.center;

		// 現在の軸成分だけで方向を判定
		float dirCheck = (myCenter - otherCenter).Dot(pushDir);
		if (dirCheck < 0) { pushDir = -pushDir; }

		// 押し出しベクトル
		Vec3 separation = pushDir * (minOverlap + 0.001f); // 浮動小数点誤差のために少し余分に

		// 位置を修正
		mScene->SetWorldPos(mScene->GetWorldPos() + separation);
		UpdateHullDimensions(); // ハル位置更新

		// 押し出された方向の速度を殺す（壁に押し続けられているときに速度が蓄積するのを防ぐ）
		float velProjected = mData.velocity.Dot(pushDir);
		if (velProjected < 0) { mData.velocity -= pushDir * velProjected; }
	}
}

/// @brief 速度を法線に沿ってクリップする
/// @param vel 入力速度
/// @param normal クリップ法線
/// @param overbounce オーバーバウンス係数
/// @return クリップ後の速度
Vec3 MovementComponent::ClipVelocity(
	const Vec3& vel, const Vec3& normal,
	float       overbounce
) {
	// Source/Quake PM_ClipVelocity
	const float backoff = vel.Dot(normal) * overbounce;
	Vec3        out     = vel - normal * backoff;
	// Numerical cleanup to avoid jitter at tiny scales
	if (std::fabs(out.x) < 1e-7f) out.x = 0.0f;
	if (std::fabs(out.y) < 1e-7f) out.y = 0.0f;
	if (std::fabs(out.z) < 1e-7f) out.z = 0.0f;
	return out;
}

/// @brief 足元位置からハル（当たり判定ボックス）を構築する
/// @param feetPos 足元の位置
/// @return ハル（Box）
Unnamed::Box MovementComponent::BuildHullAtFeet(const Vec3& feetPos) const {
	return Unnamed::Box{
		.center = feetPos + Vec3::up * Math::HtoM(mData.currentHeightHu * 0.5f),
		.halfSize = Math::HtoM(
			{
				mData.currentWidthHu * 0.5f,
				mData.currentHeightHu * 0.5f,
				mData.currentWidthHu * 0.5f,
			}
		)
	};
}

/// @brief スライド移動（衝突処理のメインロジック）
/// @param position [in/out] 足元の位置
/// @param velocity [in/out] 速度ベクトル
/// @param timeTotal 移動時間
/// @return 衝突した平面の数
int MovementComponent::SlideMove(
	Vec3&       position,
	Vec3&       velocity,
	const float timeTotal
) {
	float timeLeft = std::max(0.0f, timeTotal);

	std::array<Vec3, kMaxClipPlanes> planes{};
	int                              numplanes = 0;

	for (int bumpcount = 0; bumpcount < kMaxBumps && timeLeft > 0.0f; ++
	     bumpcount) {
		Unnamed::Box box = BuildHullAtFeet(position);

		Vec3  move    = velocity * timeLeft;
		float moveLen = move.Length();
		if (moveLen <= 1e-7f) break;

		Vec3  dir     = move / moveLen;
		float castLen = moveLen + CastSkinM();

		UPhysics::Hit hit{};
		if (!mUPhysicsEngine->BoxCast(box, dir, castLen, &hit)) {
			// 自由飛行
			position += move;
			break;
		}

		// 接触点まで移動（わずかな隙間を残す）
		const float travel  = std::clamp(hit.t, 0.0f, castLen);
		const float allowed = std::min(
			moveLen,
			std::max(0.0f, travel - SkinM())
		);
		float usedFrac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;
		// 無限ループを回避するために前進を保証
		usedFrac = std::clamp(usedFrac, kFracEps, 1.0f);

		position += dir * allowed;
		timeLeft *= (1.0f - usedFrac);

		// 衝突平面を記録
		Vec3 normal = hit.normal;
		if (!normal.IsZero()) normal.Normalize();

		// この平面が既に記録されているかチェック
		int i;
		for (i = 0; i < numplanes; i++) {
			if (planes[i].Dot(normal) > 0.99f) {
				// すでに記録済み
				velocity += normal;
				break;
			}
		}
		if (i < numplanes) continue;

		if (numplanes < kMaxClipPlanes) { planes[numplanes++] = normal; }

		// 速度を修正: すべての記録された平面に対してクリップ
		if (numplanes == 1) {
			// 単一平面: 速度をクリップ
			velocity = ClipVelocity(velocity, planes[0], 1.0f);
		} else {
			// 複数平面: すべての平面に対してクリップを試みる
			int j;
			for (j = 0; j < numplanes; j++) {
				velocity = ClipVelocity(velocity, planes[j], 1.0f);

				// まだ他の平面に向かっているかチェック
				int k;
				for (k = 0; k < numplanes; k++) {
					if (k == j) continue;
					if (velocity.Dot(planes[k]) < 0) break;
				}
				if (k == numplanes) break; // すべての平面をクリア
			}

			if (j == numplanes) {
				// 2つの平面の折り目に沿って移動
				if (numplanes != 2) {
					// 完全にブロック
					velocity = Vec3::zero;
					break;
				}
				Vec3 planeDir = planes[0].Cross(planes[1]);
				planeDir.Normalize();
				velocity = planeDir * velocity.Dot(planeDir);
			}
		}
	}

	return numplanes;
}

/// @brief ステップアップ移動
/// @param position [in/out] 足元の位置
/// @param velocity [in/out] 速度ベクトル
/// @param timeTotal 移動時間
void MovementComponent::StepMove(
	Vec3&       position,
	Vec3&       velocity,
	const float timeTotal
) {
	const Vec3 startPos = position;
	const Vec3 startVel = velocity;

	// まず通常のスライド移動を試す
	SlideMove(position, velocity, timeTotal);

	// 水平移動距離を計算
	Vec3  down    = position;
	float downVel = velocity.y;

	// 元の位置からステップアップを試す
	position = startPos;
	velocity = startVel;

	// ステップアップ
	position += Vec3::up * StepHeightM();

	// オーバーラップチェック
	Unnamed::Box  boxUp = BuildHullAtFeet(position);
	UPhysics::Hit ov{};
	if (mUPhysicsEngine->BoxOverlap(boxUp, &ov)) {
		// ステップアップできない（上に障害物）
		position   = down;
		velocity.y = downVel;
		return;
	}

	// ステップアップした位置からスライド移動
	SlideMove(position, velocity, timeTotal);

	// ステップダウン
	Unnamed::Box  boxAt = BuildHullAtFeet(position);
	UPhysics::Hit downHit{};
	if (mUPhysicsEngine->BoxCast(
		boxAt, -Vec3::up,
		StepHeightM() + RestOffsetM(), &downHit
	)) {
		// 立てる地面か?
		const float threshold = mData.groundNormalY;
		if (downHit.normal.y >= threshold) {
			float drop = std::max(0.0f, downHit.t - RestOffsetM());
			position   += -Vec3::up * drop;
		}
	}

	// 水平移動距離を比較
	const float downDist = (
		Vec3(down.x - startPos.x, 0.0f, down.z - startPos.z)).Length();
	const float upDist = (Vec3(
		position.x - startPos.x, 0.0f,
		position.z - startPos.z
	)).Length();

	// ステップアップの方が進んでいない場合は元に戻す
	if (downDist >= upDist) {
		position   = down;
		velocity.y = downVel;
	}
}

/// @brief 地面チェックとスナップ
/// @param position [in/out] 足元の位置（接地時はスナップされる）
/// @return 接地しているか
bool MovementComponent::GroundCheck(Vec3& position) {
	// ジャンプ直後や上昇中はスナップしない
	if (mData.jumpSnapDisableTime > 0.0f || mData.velocity.y > 0.0f) {
		return false;
	}

	Unnamed::Box  box = BuildHullAtFeet(position);
	UPhysics::Hit gHit{};

	// 探索距離の決定
	float snapRange;
	if (mData.wasGroundedLastFrame) {
		snapRange = RestOffsetM() + std::max(MaxAdhesionM(), StepHeightM());
	} else { snapRange = RestOffsetM() + CastSkinM(); }

	// 地面検出
	if (!mUPhysicsEngine->BoxCast(box, Vec3::down, snapRange, &gHit)) {
		return false;
	}

	// 立てる角度か?
	const float threshold = mData.groundNormalY;
	if (gHit.normal.y < threshold) { return false; }

	// 立てる地面なのでスナップ
	const float drop = std::max(0.0f, gHit.t - RestOffsetM());
	position         += -Vec3::up * drop;

	mData.lastGroundNormal = gHit.normal;
	mData.lastGroundDistM  = gHit.t;

	return true;
}

/// @brief 衝突判定付きで移動を行う
/// @param dt 経過時間
void MovementComponent::MoveWithCollisions(const float dt) {
	// 物理エンジンがない場合、自由に移動する
	if (!mUPhysicsEngine) {
		mScene->SetWorldPos(mScene->GetWorldPos() + mData.velocity * dt);
		mData.isGrounded = false;
		UpdateHullDimensions();
		return;
	}

	// スタック状態の解決
	ResolvePenetration();

	// 位置と速度をコピー
	Vec3 position = mScene->GetWorldPos();
	Vec3 velocity = mData.velocity;

	// 前フレームで接地しており、水平に移動している場合のステップテスト
	const float horizVelSqr = velocity.x * velocity.x + velocity.z * velocity.z;
	const bool  wantStep = mData.wasGroundedLastFrame && (horizVelSqr > 1e-8f);

	if (wantStep) { StepMove(position, velocity, dt); } else {
		SlideMove(position, velocity, dt);
	}

	// 最終位置で地面をチェックしてスナップ
	bool isGrounded = GroundCheck(position);

	// 位置・速度・接地状態を更新
	mScene->SetWorldPos(position);
	mData.velocity   = velocity;
	mData.isGrounded = isGrounded;

	// 移動後もめり込み解消
	UpdateHullDimensions();
	ResolvePenetration();

	if (mData.isGrounded && mData.velocity.y < 0.0f) {
		mData.velocity.y = 0.0f;
	}

	if (!mData.isWallRunning && !mData.isSliding) {
		mData.state = mData.isGrounded ?
			              MOVEMENT_STATE::GROUND :
			              MOVEMENT_STATE::AIR;
	}

	UpdateHullDimensions();
}

/// @brief スタック状態の検出と解決
/// @param dt 経過時間
void MovementComponent::DetectAndResolveStuck(const float dt) {
	Vec3 currentPos = mScene->GetWorldPos();

	// 移動距離を計算
	float distMoved = (currentPos - mData.lastPosition).Length();

	// 入力があるかチェック
	const bool hasInput = (mData.vecMoveInput.x != 0.0f || mData.vecMoveInput.y
	                       != 0.0f) || mData.wishJump;

	// スタック判定：入力があるのにほとんど動いていない
	if (hasInput && distMoved < kStuckThreshold * dt) {
		mData.stuckTime += dt;

		// 一定時間スタックしたら脱出を試みる
		if (mData.stuckTime >= kStuckTimeThreshold) {
			mData.isStuck = true;

			// 上方向に押し出す（複数の方向を試す）
			Vec3 escapeAttempts[] = {
				Vec3::up * kStuckEscapeForce,
				Vec3(1, 2, 0).Normalized() * kStuckEscapeForce,
				Vec3(-1, 2, 0).Normalized() * kStuckEscapeForce,
				Vec3(0, 2, 1).Normalized() * kStuckEscapeForce,
				Vec3(0, 2, -1).Normalized() * kStuckEscapeForce,
			};

			bool escaped = false;
			for (const Vec3& escapeVel : escapeAttempts) {
				// 脱出方向に少し移動を試みる
				Vec3 testPos = currentPos + escapeVel * dt * 2.0f;
				mScene->SetWorldPos(testPos);
				UpdateHullDimensions();

				// オーバーラップチェック
				if (mUPhysicsEngine) {
					UPhysics::Hit ov{};
					if (!mUPhysicsEngine->BoxOverlap(mHull, &ov)) {
						// 脱出成功
						mData.velocity += escapeVel;
						escaped        = true;
						break;
					}
				}
			}

			if (!escaped) {
				// すべての方向で失敗した場合、元の位置に戻す
				mScene->SetWorldPos(currentPos);
				UpdateHullDimensions();
			}

			// タイマーをリセット
			mData.stuckTime = 0.0f;
		}
	} else {
		// 正常に移動している
		mData.stuckTime = std::max(0.0f, mData.stuckTime - dt * 2.0f);
		if (mData.stuckTime == 0.0f) { mData.isStuck = false; }
	}

	// 現在位置を記録
	mData.lastPosition = mScene->GetWorldPos();
}

/// @brief ウォールラン可能かを判定する
/// @return ウォールラン可能ならtrueを返す
bool MovementComponent::CanWallrun() const {
	// しゃがみボタンが押されている場合はウォールラン不可
	if (mData.wishCrouch) return false;

	// 最小速度があるか
	const float velHorzSqr = mData.velocity.x * mData.velocity.x + mData.
	                         velocity.z * mData.velocity.z;
	const float minSpeedM = Math::HtoM(kWallrunMinSpeed);
	if (velHorzSqr < minSpeedM * minSpeedM) return false;

	// クールダウン中でないか
	if (mData.timeSinceLastWallRun < kWallrunCooldown) return false;

	// 地上に接地していないか
	if (mData.isGrounded) { return false; }

	return true;
}

/// @brief ウォールランを開始しようとする
/// @return ウォールランを開始した場合はtrueを返す
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

	// 左右両方向にレイキャスト
	const Vec3  checkDirections[] = {right, -right};
	const float checkDistance = Math::HtoM(mData.currentWidthHu * 0.5f + 10.0f);
	// 壁から少し離れてもOK

	for (const Vec3& dir : checkDirections) {
		UPhysics::Hit hit{};

		if (mUPhysicsEngine->BoxCast(mHull, dir, checkDistance, &hit)) {
			Vec3 wallNormal = hit.normal.Normalized();

			// 壁が十分垂直か（y成分が小さい）
			if (std::abs(wallNormal.y) > 0.2f) continue;

			// 同じ壁のクールダウン中か
			if (mData.timeSinceLastWallRun < kWallrunSameWallCooldown &&
			    wallNormal.Dot(mData.lastWallRunNormal) > 0.9f) { continue; }

			// Wallrun開始
			mData.isWallRunning = true;
			mData.wallRunNormal = wallNormal;
			mData.wallRunTime   = 0.0f;
			mData.state         = MOVEMENT_STATE::WALL_RUN;

			// ウォールラン開始時のジャンプ状態を記録（バニーホップ対策）
			mData.wallRunJumpWasPressed = mData.wishJump;

			// ウォールラン開始でダブルジャンプリセット
			mData.hasDoubleJump = true;

			// 速度を壁に沿った方向に調整
			Vec3 velHorz       = mData.velocity;
			velHorz.y          = 0;
			float currentSpeed = velHorz.Length();

			// 壁に沿った方向を計算（上ベクトルと壁法線の外積）
			Vec3 along = Vec3::up.Cross(wallNormal).Normalized();

			// カメラ方向と同じ向きに揃える
			// これにより、±X方向の壁でも正しく動作する
			if (along.Dot(camForward) < 0) { along = -along; }
			mData.wallRunDirection = along;

			// 現在の速度を壁に沿った方向に投影
			// これにより、元の速度を可能な限り保持
			float alongSpeed = velHorz.Dot(mData.wallRunDirection);
			if (std::abs(alongSpeed) > 1e-3f) {
				// 壁に沿った方向の速度を使用
				mData.velocity = mData.wallRunDirection * std::abs(alongSpeed);
			} else {
				// 速度がほぼ0の場合、現在の速度をそのまま使用
				mData.velocity = mData.wallRunDirection * currentSpeed;
			}

			// 垂直速度の処理（地上ジャンプからの跳ね防止）
			float originalY = mData.velocity.y;
			if (originalY > 0) {
				// 上昇中（地上ジャンプからの場合）は大幅に減衰
				mData.velocity.y = originalY * kWallrunVerticalDamping;
			} else if (originalY < 0) {
				// 落下中は軽めに減衰
				mData.velocity.y = originalY * 0.3f;
			}

			// ウォールラン開始時に小さなブースト
			float boostAmount = Math::HtoM(50.0f); // 50 HU/sのブースト
			mData.velocity    += mData.wallRunDirection * boostAmount;

			return true;
		}
	}

	return false;
}

/// @brief ウォールラン中の更新処理
/// @param dt 経過時間
void MovementComponent::UpdateWallrun(float dt) {
	mData.wallRunTime += dt;

	// 最大時間を超えたら終了
	if (mData.wallRunTime >= kWallrunMaxTime) {
		EndWallrun();
		return;
	}

	// 壁がまだあるかチェック
	if (mUPhysicsEngine) {
		const float checkDistance = Math::HtoM(
			mData.currentWidthHu * 0.5f + 20.0f
		);
		UPhysics::Hit hit{};

		// 壁に向かって（法線の逆方向）キャスト
		Vec3 toWall = -mData.wallRunNormal;
		if (!mUPhysicsEngine->
			BoxCast(mHull, toWall, checkDistance, &hit)) {
			// 壁から離れた
			EndWallrun();
			return;
		}

		// 壁の法線が変わりすぎたら終了（±X軸の壁対策で緩和）
		Vec3  newNormal = hit.normal.Normalized();
		float normalDot = newNormal.Dot(mData.wallRunNormal);

		if (normalDot < 0.5f) {
			EndWallrun();
			return;
		}

		// 法線を徐々に更新
		mData.wallRunNormal = (mData.wallRunNormal * 0.8f + newNormal * 0.2f).
			Normalized();

		// 壁走り方向も再計算
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

	// しゃがみボタンが押されたら離脱
	if (mData.wishCrouch) {
		EndWallrun();
		return;
	}

	// 地面に接地したら終了
	if (mData.isGrounded) {
		EndWallrun();
		return;
	}

	// 速度が遅すぎたら終了
	Vec3 velHorz = mData.velocity;
	velHorz.y    = 0;
	if (Math::MtoH(velHorz.Length()) < kWallrunMinSpeed * 0.5f) {
		EndWallrun();
		return;
	}

	if (kWallrunDetachOnSideInput && std::abs(mData.vecMoveInput.x) > 0.5f) {
		// 壁が左右どちらにあるか判定
		if (const auto cam = CameraManager::GetActiveCamera()) {
			Vec3 f           = cam->GetViewMat().Inverse().GetForward();
			f.y              = 0;
			const float fLen = f.Length();
			if (fLen > 1e-6f) {
				const Vec3  camForward = f * (1.0f / fLen);
				const Vec3  camRight = Vec3::up.Cross(camForward).Normalized();
				const float wallSide = camRight.Dot(mData.wallRunNormal);

				// 壁から離れる方向への入力で離脱
				// wallSide > 0 なら壁は右側 -> 右入力(x>0)で離脱
				// wallSide < 0 なら壁は左側 -> 左入力(x<0)で離脱
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

/// @brief ウォールラン中の移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Wallrun(const float wishspeed, const float dt) {
	const Vec3 wishdir = mData.wallRunDirection;
	// 前進入力があるか
	if (mData.vecMoveInput.y > 0) {
		// 加速
		const float currentSpeed = Math::MtoH(mData.velocity.Dot(wishdir));
		const float addSpeed     = wishspeed * 1.2f - currentSpeed;

		if (addSpeed > 0) {
			const float accel = ConVarManager::GetConVar("sv_airaccelerate")->
			                    GetValueAsFloat() * 1.5f;
			const float accelspeed = std::min(accel * wishspeed * dt, addSpeed);
			mData.velocity         += Math::HtoM(accelspeed) * wishdir;
		}
	} else {
		// 前進入力がない場合は摩擦で減速
		const float speedM = mData.velocity.Length();
		const float speed  = Math::MtoH(speedM);
		if (speed > 0.1f) {
			const float fric = ConVarManager::GetConVar("sv_friction")->
				GetValueAsFloat();
			const float drop = speed * fric * dt * 0.5f; // 壁では摩擦が弱め
			const float news = std::max(0.0f, speed - drop);
			if (news != speed) { mData.velocity *= (news / speed); }
		}
	}

	// 壁に向かう速度成分を除去
	const float intoWall = mData.velocity.Dot(-mData.wallRunNormal);
	if (intoWall > 0) { mData.velocity += mData.wallRunNormal * intoWall; }

	// 壁に軽く吸い付く力を追加
	const float pullForce = Math::HtoM(80.0f); // 80 HU/s
	mData.velocity        += -mData.wallRunNormal * pullForce * dt;
}

/// @brief スライド可能かを判定する
/// @return スライド可能ならtrueを返す
bool MovementComponent::CanSlide() const {
	if (!mData.isGrounded) return false;
	if (!mData.wishCrouch) return false;

	// 水平速度が十分にあるか
	const float velHorzSqr = mData.velocity.x * mData.velocity.x + mData.
	                         velocity.z * mData.velocity.z;
	const float minSpeedM = Math::HtoM(kSlideMinSpeed);

	return velHorzSqr >= minSpeedM * minSpeedM;
}

/// @brief スライドを開始しようとする
void MovementComponent::TryStartSlide() {
	if (!CanSlide()) return;

	// 現在の移動方向をスライディング方向として記録
	Vec3 velHorz = mData.velocity;
	velHorz.y    = 0;

	if (velHorz.IsZero()) return;

	mData.slideDirection = velHorz.Normalized();
	mData.isSliding      = true;
	mData.slideTime      = 0.0f;
	mData.state          = MOVEMENT_STATE::SLIDE;

	// スライディング開始時に小さなブースト
	const float currentSpeed = velHorz.Length();
	float       boostedSpeed = currentSpeed + Math::HtoM(kSlideBoostSpeed);

	// 速度キャップを適用 (スライドホップでケツワープもどきを防止)
	const float speedCapM = Math::HtoM(kSlideHopSpeedCap);
	boostedSpeed          = std::min(boostedSpeed, speedCapM);

	const float originalY = mData.velocity.y;
	mData.velocity        = mData.slideDirection * boostedSpeed;
	mData.velocity.y      = originalY; // Y軸は維持
}

/// @brief スライド中の更新処理
/// @param dt 経過時間
void MovementComponent::UpdateSlide(const float dt) {
	mData.slideTime += dt;

	// 最大時間を超えたら終了
	if (mData.slideTime >= kSlideMaxTime) {
		EndSlide();
		return;
	}

	// 地上から離れたら終了
	if (!mData.isGrounded) {
		EndSlide();
		return;
	}

	// 速度が低すぎたら終了
	Vec3 velHoriz = mData.velocity;
	velHoriz.y    = 0;
	float speed   = Math::MtoH(velHoriz.Length());
	if (speed < kSlideStopSpeed) {
		EndSlide();
		return;
	}

	// しゃがみが離されたら終了
	if (!mData.wishCrouch) { EndSlide(); }
}

/// @brief スライドを終了する
void MovementComponent::EndSlide() {
	if (!mData.isSliding) return;

	mData.isSliding      = false;
	mData.slideDirection = Vec3::zero;
	mData.slideTime      = 0.0f;

	// ハルサイズを元に戻す
	if (mData.isGrounded) { mData.state = MOVEMENT_STATE::GROUND; } else {
		mData.state = MOVEMENT_STATE::AIR;
	}
}

/// @brief スライド中の移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Slide(
	const float wishspeed, const float dt
) {
	// 通常時より低い摩擦を適用
	Friction(kSlideFriction, dt);

	// accel値は低めに
	if (!mData.wishDirection.IsZero()) {
		Vec3 wishDir = mData.wishDirection;
		wishDir.y    = 0;
		if (!wishDir.IsZero()) {
			wishDir.Normalize();
			Accelerate(wishDir, wishspeed, kSlideAcceleration, dt);
		}
	}
}
