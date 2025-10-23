#include "MovementComponent.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <array>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/Transform/SceneComponent.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/OldConsole/ConVarManager.h>

#include "engine/Input/InputSystem.h"


/// @brief コンストラクタ
/// @param width プレイヤーの幅
/// @param height プレイヤーの高さ
MovementData::MovementData(float width, float height) : currentWidthHu(width),
	currentHeightHu(height) {
	defaultHeightHu = height;
	crouchHeightHu  = height * 0.75f;
}

/// @brief デフォルトコンストラクタ
MovementData::MovementData() : currentWidthHu(32.0f), currentHeightHu(72.0f) {
	defaultHeightHu = currentHeightHu;
	crouchHeightHu  = currentHeightHu * 0.75f;
}

/// @brief コンポーネントがエンティティにアタッチされたときの処理
/// @param owner 所有エンティティ
void MovementComponent::OnAttach(Entity& owner) { Component::OnAttach(owner); }

/// @brief 初期化
/// @param uphysics UPhysicsエンジンポインタ
/// @param md 移動データ
void MovementComponent::Init(UPhysics::Engine*   uphysics,
                             const MovementData& md) {
	mUPhysicsEngine             = uphysics;
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
	UpdateHullDimensions();
}

/// @brief 物理演算前の更新
/// @param deltaTime 経過時間
void MovementComponent::PrePhysics(float) {
	ProcessInput();

	Debug::DrawBox(
		mData.hull.center,
		Quaternion::identity,
		mData.hull.halfSize * 2.0f,
		{0.34f, 0.66f, 0.95f, 1.0f}
	);
	Debug::DrawArrow(
		mData.hull.center,
		mData.velocity * 0.25f,
		Vec4::yellow,
		0.05f
	);
}

/// @brief 更新
/// @param dt 経過時間
void MovementComponent::Update(const float dt) {
	ProcessMovement(dt);
}

/// @brief 物理演算後の更新
/// @param deltaTime 経過時間
void MovementComponent::PostPhysics(float) {
}

/// @brief インスペクタ内のImGui描画
void MovementComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("MovementComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("State: %s", ToString(mData.state));
		ImGuiWidgets::DragVec3("Velocity", mData.velocity, Vec3::zero, 0.1f,
		                       "%.3f");
		ImGui::Checkbox("Grounded", &mData.isGrounded);
		ImGui::Text("HeightHU: %.2f  WidthHU: %.2f", mData.currentHeightHu,
		            mData.currentWidthHu);

		// Wallrun info
		if (mData.isWallRunning) {
			ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "WALLRUNNING! (%.2fs)",
			                   mData.wallRunTime);
			ImGuiWidgets::DragVec3("WallNormal", mData.wallRunNormal,
			                       Vec3::zero, 0.1f, "%.3f");
		} else {
			ImGui::Text("Time since wallrun: %.2fs",
			            mData.timeSinceLastWallRun);
		}

		if (mData.isStuck) {
			ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "STUCK! (%.2fs)",
			                   mData.stuckTime);
		} else {
			ImGui::Text("Stuck Timer: %.2fs", mData.stuckTime);
		}

		// Ground slope thresholds
		if (ImGui::TreeNode("Ground Slope Settings")) {
			ImGui::SliderFloat("Enter Threshold",
			                   &mData.groundEnterSlopeThreshold, 0.0f, 1.0f,
			                   "%.3f");
			ImGui::Text("  (接地開始: cos(45°) = 0.707)");
			ImGui::SliderFloat("Leave Threshold",
			                   &mData.groundLeaveSlopeThreshold, 0.0f, 1.0f,
			                   "%.3f");
			ImGui::Text("  (接地解除: cos(60°) = 0.5)");
			if (ImGui::Button("Reset to Default")) {
				mData.groundEnterSlopeThreshold = 0.707f;
				mData.groundLeaveSlopeThreshold = 0.5f;
			}
			ImGui::TreePop();
		}
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
		mData.currentHeightHu - 8.0f);
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

/// @brief 入力処理
void MovementComponent::ProcessInput() {
	mData.vecMoveInput = Vec2::zero;
	if (InputSystem::IsPressed("forward")) mData.vecMoveInput.y += 1.0f;
	if (InputSystem::IsPressed("back")) mData.vecMoveInput.y -= 1.0f;
	if (InputSystem::IsPressed("moveright")) mData.vecMoveInput.x += 1.0f;
	if (InputSystem::IsPressed("moveleft")) mData.vecMoveInput.x -= 1.0f;
	if (mData.vecMoveInput.SqrLength() > 1.0f) mData.vecMoveInput.Normalize();

	Vec3 wish = Vec3::zero;
	if (auto cam = CameraManager::GetActiveCamera()) {
		Vec3 f = cam->GetViewMat().Inverse().GetForward();
		f.y    = 0.0f;
		if (!f.IsZero()) f.Normalize();
		Vec3 r = Vec3::up.Cross(f).Normalized();
		wish   = f * mData.vecMoveInput.y + r * mData.vecMoveInput.x;
		wish.y = 0.0f;
		if (!wish.IsZero()) wish.Normalize();
	}
	mData.wishDirection = wish;
	mData.wishJump      = InputSystem::IsPressed("jump");
	mData.wishCrouch    = InputSystem::IsPressed("crouch");
}

/// @brief 移動処理
/// @param dt 経過時間
void MovementComponent::ProcessMovement(const float dt) {
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
			.center = posFeet + Vec3::up * Math::HtoM(targetHU * 0.5f),
			.halfSize = Math::HtoM({
				mData.currentWidthHu * 0.5f, targetHU * 0.5f,
				mData.currentWidthHu * 0.5f
			})
		};
		UPhysics::Hit ov{};
		const bool    blocked = mUPhysicsEngine && mUPhysicsEngine->BoxOverlap(
			test, &ov);
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

	if (mData.isGrounded && !mData.isSliding && CanSlide()) {
		TryStartSlide();
	}

	// スライディング更新
	if (mData.isSliding) {
		UpdateSlide(dt);
	}

	if (mData.wishJump && mData.isGrounded) {
		mData.velocity.y    = Math::HtoM(kJumpVelocityHu);
		mData.isGrounded    = false;
		mData.state         = MOVEMENT_STATE::AIR;
		mData.hasDoubleJump = true;

		// スライディング中のジャンプでスライディング終了
		if (mData.isSliding) {
			// スライドホップ時の速度キャップを適用
			Vec3 velHorz          = mData.velocity;
			velHorz.y             = 0;
			const float horzSpeed = velHorz.Length();
			const float speedCapM = Math::HtoM(kSlideHopSpeedCap);
			if (horzSpeed > speedCapM) {
				mData.velocity.x *= (speedCapM / horzSpeed);
				mData.velocity.z *= (speedCapM / horzSpeed);
			}
			EndSlide();
		}
	}

	const float wishspeed = mData.wishDirection.IsZero() ?
		                        0.0f :
		                        mData.currentSpeed;

	mData.timeSinceLastWallRun += dt;
	if (!mData.isGrounded && !mData.isWallRunning && CanWallrun()) {
		TryStartWallrun();
	}

	// ジャンプキーが離されてから押された検出（バニーホップ対策）
	const bool jumpPressed = mData.wishJump && !mData.lastFrameWishJump;

	if (mData.wishJump) {
		if (mData.isGrounded) {
			// 地上ジャンプ
			mData.velocity.y    = Math::HtoM(kJumpVelocityHu);
			mData.isGrounded    = false;
			mData.state         = MOVEMENT_STATE::AIR;
			mData.hasDoubleJump = true; // 地上ジャンプでダブルジャンプリセット
		} else if (mData.isWallRunning && !mData.wallRunJumpWasPressed) {
			// Wallrun jump: キーを一度離してから再度押した場合のみ
			// (バニーホップでの誤発動防止)
			// 壁方向入力は完全に無視（常にジャンプ可能）

			// 現在の進行方向の速度を取得
			Vec3 forwardVel = mData.wallRunDirection * mData.velocity.Dot(
				mData.wallRunDirection);

			// 壁から離れる方向（横と上）
			Vec3 awayDir = mData.wallRunNormal * 0.7f + Vec3::up * 1.0f;
			awayDir.Normalize();

			// 進行方向の速度 + 壁から離れるブースト
			mData.velocity = forwardVel + awayDir * Math::HtoM(
				kWallrunJumpForce);

			mData.hasDoubleJump = true; // ウォールランジャンプでダブルジャンプリセット
			EndWallrun();
		} else if (!mData.isGrounded && !mData.isWallRunning && mData.
			hasDoubleJump && jumpPressed) {
			// ダブルジャンプ（空中で、キーを離してから押した場合）
			mData.velocity.y    = Math::HtoM(kDoubleJumpVelocityHu);
			mData.hasDoubleJump = false; // 使用済み
		}
	} else {
		// ジャンプキーが離されたらフラグをリセット
		if (mData.isWallRunning) {
			mData.wallRunJumpWasPressed = false;
		}
	}

	// 前フレームのジャンプ入力を保存
	mData.lastFrameWishJump = mData.wishJump;

	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();

	if (mData.isWallRunning) {
		// Wallrun gravity (reduced)
		mData.velocity.y -= Math::HtoM(g) * kWallrunGravityScale * dt;
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

	// 衝突付き移動
	MoveWithCollisions(dt);

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
	} else {
		mData.justLanded = false;
	}

	CheckForNaNAndClamp(); // NaNチェックと速度クランプ
}

/// @brief 地上での移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Ground(const float wishspeed, const float dt) {
	// Source Engine方式：2Dで速度計算、後で押し上げ/押し下げで対応
	// Y成分は0にして水平移動のみ計算
	mData.velocity.y = 0.0f;

	Friction(dt); // 摩擦を適用

	Vec3 wishdir = mData.wishDirection;
	wishdir.y    = 0.0f; // 水平方向のみ

	if (!wishdir.IsZero() && wishspeed > 0.0f) {
		wishdir.Normalize();
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
/// @param dt 経過時間
void MovementComponent::Friction(float dt) {
	if (!mData.isGrounded) return;

	// Quake/Source: 水平速度のみで摩擦を計算
	Vec3 vel_horz     = mData.velocity;
	vel_horz.y        = 0;
	const float speed = Math::MtoH(vel_horz.Length());
	if (speed < 0.1f) return;

	const float fric = ConVarManager::GetConVar("sv_friction")->
		GetValueAsFloat();
	const float stop = ConVarManager::GetConVar("sv_stopspeed")->
		GetValueAsFloat();
	const float ctrl = speed < stop ? stop : speed;

	const float drop = ctrl * fric * dt;

	float newspeed = std::max(0.0f, speed - drop);

	if (newspeed != speed) {
		newspeed /= speed;
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
	float acc = std::min(accel * speed * dt, add);
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
	mData.velocity += Math::HtoM(acc) * dir;
}

/// @brief ハル(当たり判定)の寸法を更新
void MovementComponent::UpdateHullDimensions() {
	// 足元原点を前提に、中心を高さの半分だけ上げる
	mData.hull = {
		.center = mScene->GetWorldPos() + Vec3::up * Math::HtoM(
			mData.currentHeightHu * 0.5f),
		.halfSize = Math::HtoM({
			mData.currentWidthHu * 0.5f,
			mData.currentHeightHu * 0.5f,
			mData.currentWidthHu * 0.5f
		})
	};
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

namespace {
	/// @brief 速度を法線に沿ってクリップする
	/// @param vel 入力速度
	/// @param normal クリップ法線
	/// @param overbounce オーバーバウンス係数
	/// @return クリップ後の速度
	Vec3 ClipVelocity(const Vec3& vel, const Vec3& normal, float overbounce) {
		// Source/Quake PM_ClipVelocity
		const float backoff = vel.Dot(normal) * overbounce;
		Vec3        out     = vel - normal * backoff;
		// Numerical cleanup to avoid jitter at tiny scales
		if (std::fabs(out.x) < 1e-7f) out.x = 0.0f;
		if (std::fabs(out.y) < 1e-7f) out.y = 0.0f;
		if (std::fabs(out.z) < 1e-7f) out.z = 0.0f;
		return out;
	}
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

	const auto buildHullAtFeet = [&](const Vec3& feetPos) -> Unnamed::Box {
		return Unnamed::Box{
			.center = feetPos + Vec3::up * Math::HtoM(
				mData.currentHeightHu * 0.5f),
			.halfSize = Math::HtoM({
				mData.currentWidthHu * 0.5f,
				mData.currentHeightHu * 0.5f,
				mData.currentWidthHu * 0.5f,
			})
		};
	};

	auto slideMove = [&](const Vec3& startFeet,
	                     const Vec3& vIn,
	                     const float timeTotal) -> std::tuple<
		Vec3, Vec3, bool> {
		Vec3  posFeet  = startFeet;
		Vec3  vel      = vIn;
		float timeLeft = std::max(0.0f, timeTotal);
		bool  anyHit   = false;

		std::array<Vec3, kMaxClipPlanes> planes{};
		int                              planeCount = 0;

		for (int bump = 0; bump < kMaxBumps && timeLeft > 0.0f; ++bump) {
			Unnamed::Box box = buildHullAtFeet(posFeet);

			Vec3  move    = vel * timeLeft;
			float moveLen = move.Length();
			if (moveLen <= 1e-7f) break;

			Vec3  dir     = move / moveLen;
			float castLen = moveLen + CastSkinM();

			UPhysics::Hit hit{};
			if (!mUPhysicsEngine->BoxCast(box, dir, castLen, &hit)) {
				// 自由飛行
				posFeet += move;
				timeLeft = 0.0f;
				break;
			}

			anyHit = true;

			// 接触点まで移動（わずかな隙間を残す）
			const float travel  = std::clamp(hit.t, 0.0f, castLen);
			const float allowed = std::min(moveLen,
			                               std::max(0.0f, travel - SkinM()));
			float usedFrac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;
			// 無限ループを回避するために前進を保証
			usedFrac = std::clamp(usedFrac, kFracEps, 1.0f);

			posFeet += dir * (allowed);
			timeLeft *= (1.0f - usedFrac);

			// 衝突平面を記録
			Vec3 n = hit.normal;
			if (!n.IsZero()) n.Normalize();
			if (planeCount < kMaxClipPlanes) {
				planes[planeCount++] = n;
			}

			// 蓄積された平面に対して速度を解決する（折り目サポート）
			// このバンプステップの元々の意図から開始
			Vec3 primalVel = vel;
			// 最初に、各平面に対して速度をクリップする
			for (int i = 0; i < planeCount; ++i) {
				if (primalVel.Dot(planes[i]) < 0.0f) {
					primalVel = ClipVelocity(primalVel, planes[i], 1.001f);
				}
			}
			vel = primalVel;

			// まだどの平面にも押し込んでいる場合、2つの平面の折り目に沿ってスライドを試みる
			for (int i = 0; i < planeCount; ++i) {
				if (vel.Dot(planes[i]) >= 0.0f) continue;
				bool resolved = false;
				for (int j = 0; j < planeCount; ++j) {
					if (i == j) continue;
					Vec3        dirCrease = planes[i].Cross(planes[j]);
					const float lenSq     = dirCrease.SqrLength();
					if (lenSq < 1e-8f) continue;
					dirCrease /= std::sqrt(lenSq);
					float speedAlong = vel.Dot(dirCrease);
					vel              = dirCrease * speedAlong;
					// Ensure not into any plane now
					bool ok = true;
					for (int k = 0; k < planeCount; ++k) {
						if (vel.Dot(planes[k]) < -1e-6f) {
							ok = false;
							break;
						}
					}
					if (ok) {
						resolved = true;
						break;
					}
				}
				if (!resolved) {
					// 2つ以上の平面に挟まれて動けない場合停止
					vel = Vec3::zero;
				}
				break;
			}
		}

		return {posFeet, vel, anyHit};
	};

	Vec3 startFeet = mScene->GetWorldPos();

	// パスA: ステップなし
	auto [posA, velA, hitA] = slideMove(startFeet, mData.velocity, dt);

	// オプション: 前フレームで接地しており、水平に移動している場合のステップ試行
	Vec3 posFinal = posA;
	Vec3 velFinal = velA;

	Vec3 horizVel       = mData.velocity;
	horizVel.y          = 0.0f;
	const bool wantStep = mData.wasGroundedLastFrame && (horizVel.SqrLength() >
		1e-8f);
	if (wantStep) {
		// 登る
		Vec3          posUp = startFeet + Vec3::up * StepHeightM();
		Unnamed::Box  boxUp = buildHullAtFeet(posUp);
		UPhysics::Hit ov{};

		if (!mUPhysicsEngine->BoxOverlap(boxUp, &ov)) {
			auto [posB, velB, hitB] = slideMove(posUp, mData.velocity, dt);

			// 降りられる地面を探す
			Unnamed::Box  boxAt = buildHullAtFeet(posB);
			UPhysics::Hit downHit{};
			if (mUPhysicsEngine->BoxCast(boxAt, -Vec3::up,
			                             StepHeightM() + RestOffsetM(),
			                             &downHit)) {
				// 立てる地面か?
				const float threshold = mData.isGrounded ?
					                        mData.groundLeaveSlopeThreshold :
					                        mData.groundEnterSlopeThreshold;
				if (downHit.normal.y >= threshold) {
					float drop = std::max(0.0f, downHit.t - RestOffsetM());
					posB += -Vec3::up * drop;
				}
			}

			const float progA = (Vec3(posA.x - startFeet.x, 0.0f,
			                          posA.z - startFeet.z)).Length();
			const float progB = (Vec3(posB.x - startFeet.x, 0.0f,
			                          posB.z - startFeet.z)).Length();
			if (progB > progA + 1e-4f) {
				posFinal = posB;
				velFinal = velB;
			}
		}
	}

	// 最終位置で地面をチェックしてスナップ
	bool isGrounded = false;
	Vec3 posFeet    = posFinal;

	{
		Unnamed::Box  box = buildHullAtFeet(posFeet);
		UPhysics::Hit gHit{};
		const float   snapRange = RestOffsetM() + std::max(
			MaxAdhesionM(), StepHeightM());
		if (mUPhysicsEngine->BoxCast(box, -Vec3::up, snapRange, &gHit)) {
			// このフレームでジャンプ中（上方向に移動中）の場合はスナップしない
			if (!(mData.wishJump && mData.velocity.y > 0.0f)) {
				float drop = std::max(0.0f, gHit.t - RestOffsetM());
				posFeet += -Vec3::up * drop;
				// ヒステリシス: 接地中なら緩い閾値で判定、空中からなら厳しい閾値で判定
				const float threshold = mData.isGrounded ?
					                        mData.groundLeaveSlopeThreshold :
					                        mData.groundEnterSlopeThreshold;
				isGrounded             = (gHit.normal.y >= threshold);
				mData.lastGroundNormal = gHit.normal;
				mData.lastGroundDistM  = gHit.t;
			}
		}
	}

	// 位置・速度・接地状態を更新
	mScene->SetWorldPos(posFeet);
	mData.velocity   = velFinal;
	mData.isGrounded = isGrounded;
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
void MovementComponent::DetectAndResolveStuck(float dt) {
	Vec3 currentPos = mScene->GetWorldPos();

	// 移動距離を計算
	float distMoved = (currentPos - mData.lastPosition).Length();

	// 入力があるかチェック
	bool hasInput = !mData.vecMoveInput.IsZero() || mData.wishJump;

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
					if (!mUPhysicsEngine->BoxOverlap(mData.hull, &ov)) {
						// 脱出成功
						mData.velocity += escapeVel;
						escaped = true;
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
		if (mData.stuckTime == 0.0f) {
			mData.isStuck = false;
		}
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
	Vec3 velHorz      = mData.velocity;
	velHorz.y         = 0;
	const float speed = Math::MtoH(velHorz.Length());
	if (speed < kWallrunMinSpeed) return false;

	// クールダウン中でないか
	if (mData.timeSinceLastWallRun < kWallrunCooldown) return false;

	return true;
}

/// @brief ウォールランを開始しようとする
/// @return ウォールランを開始した場合はtrueを返す
bool MovementComponent::TryStartWallrun() {
	if (!mUPhysicsEngine) return false;

	// 左右に壁があるかチェック
	Vec3 camForward = Vec3::zero;
	if (auto cam = CameraManager::GetActiveCamera()) {
		Vec3 f = cam->GetViewMat().Inverse().GetForward();
		f.y    = 0;
		if (!f.IsZero()) f.Normalize();
		camForward = f;
	}

	if (camForward.IsZero()) return false;

	Vec3 right = Vec3::up.Cross(camForward).Normalized();

	// 左右両方向にレイキャスト
	Vec3        checkDirections[] = {right, -right};
	const float checkDistance = Math::HtoM(mData.currentWidthHu * 0.5f + 10.0f);
	// 壁から少し離れてもOK

	for (const Vec3& dir : checkDirections) {
		UPhysics::Hit hit{};
		Vec3          startPos = mData.hull.center;

		if (mUPhysicsEngine->BoxCast(mData.hull, dir, checkDistance, &hit)) {
			Vec3 wallNormal = hit.normal.Normalized();

			// 壁が十分垂直か（y成分が小さい）
			if (std::abs(wallNormal.y) > 0.2f) continue;

			// 同じ壁のクールダウン中か
			if (mData.timeSinceLastWallRun < kWallrunSameWallCooldown &&
				wallNormal.Dot(mData.lastWallRunNormal) > 0.9f) {
				continue;
			}

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
			if (along.Dot(camForward) < 0) {
				along = -along;
			}
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
			mData.velocity += mData.wallRunDirection * boostAmount;

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
			mData.currentWidthHu * 0.5f + 20.0f);
		UPhysics::Hit hit{};

		// 壁に向かって（法線の逆方向）キャスト
		Vec3 toWall = -mData.wallRunNormal;
		if (!mUPhysicsEngine->
			BoxCast(mData.hull, toWall, checkDistance, &hit)) {
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
		if (auto cam = CameraManager::GetActiveCamera()) {
			Vec3 camForward = cam->GetViewMat().Inverse().GetForward();
			camForward.y    = 0;
			if (!camForward.IsZero()) {
				camForward.Normalize();
				Vec3 projectedDir = Math::ProjectOnPlane(
					camForward, mData.wallRunNormal);
				if (!projectedDir.IsZero()) {
					mData.wallRunDirection = projectedDir.Normalized();
				}
			}
		}
	}

	// しゃがみボタンが押されたら離脱
	if (mData.wishCrouch) {
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
		Vec3 camForward = Vec3::zero;
		if (auto cam = CameraManager::GetActiveCamera()) {
			Vec3 f = cam->GetViewMat().Inverse().GetForward();
			f.y    = 0;
			if (!f.IsZero()) f.Normalize();
			camForward = f;
		}

		if (!camForward.IsZero()) {
			Vec3  camRight = Vec3::up.Cross(camForward).Normalized();
			float wallSide = camRight.Dot(mData.wallRunNormal);

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
			float accel = ConVarManager::GetConVar("sv_airaccelerate")->
				GetValueAsFloat() * 1.5f;
			float accelspeed = std::min(accel * wishspeed * dt, addSpeed);
			mData.velocity += Math::HtoM(accelspeed) * wishdir;
		}
	} else {
		// 前進入力がない場合は摩擦で減速
		float speed = Math::MtoH(mData.velocity.Length());
		if (speed > 0.1f) {
			const float fric = ConVarManager::GetConVar("sv_friction")->
				GetValueAsFloat();
			const float drop = speed * fric * dt * 0.5f; // 壁では摩擦が弱め
			const float news = std::max(0.0f, speed - drop);
			if (news != speed && speed > 0) {
				mData.velocity *= (news / speed);
			}
		}
	}

	// 壁に向かう速度成分を除去
	const float intoWall = mData.velocity.Dot(-mData.wallRunNormal);
	if (intoWall > 0) {
		mData.velocity += mData.wallRunNormal * intoWall;
	}

	// 壁に軽く吸い付く力を追加
	const float pullForce = Math::HtoM(80.0f); // 80 HU/s
	mData.velocity += -mData.wallRunNormal * pullForce * dt;
}

/// @brief スライド可能かを判定する
/// @return スライド可能ならtrueを返す
bool MovementComponent::CanSlide() const {
	if (!mData.isGrounded) return false;
	if (!mData.wishCrouch) return false;

	// 水平速度が十分にあるか
	Vec3 velHorizontal = mData.velocity;
	velHorizontal.y    = 0;
	const float speed  = Math::MtoH(velHorizontal.Length());

	return speed >= kSlideMinSpeed;
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

	// 速度キャップを適用（スライドホップの無限加速を防ぐ）
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
	if (!mData.wishCrouch) {
		EndSlide();
	}
}

/// @brief スライドを終了する
void MovementComponent::EndSlide() {
	if (!mData.isSliding) return;

	mData.isSliding      = false;
	mData.slideDirection = Vec3::zero;
	mData.slideTime      = 0.0f;

	// ハルサイズを元に戻す
	if (mData.isGrounded) {
		mData.state = MOVEMENT_STATE::GROUND;
	} else {
		mData.state = MOVEMENT_STATE::AIR;
	}
}

/// @brief スライド中の移動処理
/// @param wishspeed 目標速度
/// @param dt 経過時間
void MovementComponent::Slide([[maybe_unused]] float wishspeed, float dt) {
	// スライディング中の摩擦
	Vec3 velHorz = mData.velocity;
	velHorz.y    = 0;
	float speed  = Math::MtoH(velHorz.Length());

	if (speed > 0.1f) {
		// スライディング専用の低摩擦
		float drop     = kSlideFriction * dt;
		float newSpeed = std::max(0.0f, speed - drop);

		if (newSpeed != speed && speed > 0) {
			mData.velocity.x *= (newSpeed / speed);
			mData.velocity.z *= (newSpeed / speed);
		}
	}

	// スライディング方向への入力で少し方向転換可能
	if (!mData.wishDirection.IsZero()) {
		Vec3 wishDir = mData.wishDirection;
		wishDir.y    = 0;
		if (!wishDir.IsZero()) {
			wishDir.Normalize();

			// 現在の方向とwishDirを少しずつブレンド
			Vec3 currentDir = velHorz.Normalized();
			Vec3 newDir = (currentDir * 0.95f + wishDir * 0.05f).Normalized();

			float currentSpeed = velHorz.Length();
			mData.velocity.x   = newDir.x * currentSpeed;
			mData.velocity.z   = newDir.z * currentSpeed;
		}
	}
}
