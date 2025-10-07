#include "Movement.h"

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/Transform/SceneComponent.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/OldConsole/ConVarManager.h>
#include <algorithm>
#include <cmath>

#include "engine/Input/InputSystem.h"

namespace {
	constexpr float kJumpVelocityHu = 300.0f; // HU/s
}

// ----------------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------------
void Movement::OnAttach(Entity& owner) { Component::OnAttach(owner); }

void Movement::Init(UPhysics::Engine* uphysics, const MovementData& md) {
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
	RefreshHullFromTransform();
}

void Movement::PrePhysics(float dt) {
	(void)dt;
	ProcessInput();
}

void Movement::Update(float dt) {
	// const float maxStep = 1.0f / 120.0f;
	// int         steps   = std::max(1, (int)std::ceil(dt / maxStep));
	// float       subdt   = dt / steps;
	// for (int i = 0; i < steps; ++i) ;

	ProcessMovement(dt);

	Debug::DrawBox(
		mData.hull.center,
		Quaternion::identity,
		mData.hull.halfSize * 2.0f,
		{0.34f, 0.66f, 0.95f, 1.0f}
	);
}

void Movement::PostPhysics(float) {
}

// ----------------------------------------------------------------------------
// Debug UI
// ----------------------------------------------------------------------------
void Movement::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("State: %s", ToString(mData.state));
		ImGuiWidgets::DragVec3("Velocity", mData.velocity, Vec3::zero, 0.1f,
		                       "%.3f");
		ImGui::Checkbox("Grounded", &mData.isGrounded);
		ImGui::Text("HeightHU: %.2f  WidthHU: %.2f", mData.currentHeight,
		            mData.currentWidth);

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
	}
#endif
}

// ----------------------------------------------------------------------------
// Public accessors
// ----------------------------------------------------------------------------
Vec3& Movement::GetVelocity() { return mData.velocity; }

Vec3 Movement::GetHeadPos() const {
	// 足元原点前提：頭は currentHeight から少し下げる
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mData.currentHeight - 8.0f);
}

void Movement::SetVelocity(const Vec3& v) { mData.velocity = v; }

// ----------------------------------------------------------------------------
// Input
// ----------------------------------------------------------------------------
void Movement::ProcessInput() {
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

// ----------------------------------------------------------------------------
// Step-wise movement
// ----------------------------------------------------------------------------
void Movement::ProcessMovement(float dt) {
	// --- Height (crouch/stand) with ceiling check ---------------------------
	float targetHU = mData.wishCrouch ?
		                 mData.crouchHeight :
		                 mData.defaultHeight;
	if (targetHU > mData.currentHeight) {
		// 立ち上がる：頭上に空きがあるか確かめる
		Vec3         posFeet = mScene->GetWorldPos();
		Unnamed::Box test    = {
			.center = posFeet + Vec3::up * Math::HtoM(targetHU * 0.5f),
			.halfSize = Math::HtoM({
				mData.currentWidth * 0.5f, targetHU * 0.5f,
				mData.currentWidth * 0.5f
			})
		};
		UPhysics::Hit ov{};
		bool          blocked = (mUPhysicsEngine && mUPhysicsEngine->BoxOverlap(
			test, &ov));
		mData.currentHeight = blocked ? mData.currentHeight : targetHU;
	} else {
		// しゃがみ：常に縮めてOK
		mData.currentHeight = targetHU;
	}

	// --- Refresh hull for latest height -------------------------------------
	RefreshHullFromTransform();

	// --- Refresh grounded cache BEFORE jump ---------------------------------
	const float groundProbeLen = StepHeightM() + MaxAdhesionM() + RestOffsetM();
	RefreshGroundCache(groundProbeLen);

	// --- Speed setup ---------------------------------------------------------
	mData.currentSpeed = mData.wishCrouch ?
		                     mData.crouchSpeed :
		                     mData.sprintSpeed;

	// --- Slide attempt (地上で速度が十分にあり、クロウチが押された) ----------
	if (mData.isGrounded && !mData.isSliding && CanSlide()) {
		TryStartSlide();
	}

	// スライディング更新
	if (mData.isSliding) {
		UpdateSlide(dt);
	}

	// --- Jump ---------------------------------------------------------------
	if (mData.wishJump && mData.isGrounded) {
		mData.velocity.y = Math::HtoM(kJumpVelocityHu);
		mData.isGrounded = false;
		mData.state      = MOVEMENT_STATE::AIR;

		// スライディング中のジャンプでスライディング終了
		if (mData.isSliding) {
			// スライドホップ時の速度キャップを適用
			Vec3 vel_horz   = mData.velocity;
			vel_horz.y      = 0;
			float horzSpeed = vel_horz.Length();
			float speedCapM = Math::HtoM(kSlideHopSpeedCap);
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

	// --- Wallrun attempt (if in air and conditions met) --------------------
	mData.timeSinceLastWallRun += dt;
	if (!mData.isGrounded && !mData.isWallRunning && CanWallrun()) {
		TryStartWallrun();
	}

	// --- Jump (ground, wallrun, or double jump) -----------------------------
	// ジャンプキーが離されてから押された検出（バニーホップ対策）
	bool jumpPressed = mData.wishJump && !mData.lastFrameWishJump;

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
			//float forwardSpeed = forwardVel.Length();

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

	// --- Accelerations & gravity --------------------------------------------
	if (mData.isWallRunning) {
		// Wallrun gravity (reduced)
		const float g = ConVarManager::GetConVar("sv_gravity")->
			GetValueAsFloat();
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

	// --- Landing detection (着地直前の速度を保存) ---------------------------
	// 空中で下方向に移動している場合、着地時の速度として保存
	if (!mData.isGrounded && mData.velocity.y < 0.0f) {
		mData.lastLandingVelocityY = mData.velocity.y;
	}

	// --- Collision & stepping -----------------------------------------------
	MoveCollideAndSlide(dt);

	// --- Stuck detection & resolution ----------------------------------------
	DetectAndResolveStuck(dt);

	// --- Landing detection (着地検出) ----------------------------------------
	// 前フレームで空中、今フレームで地上 = 着地
	if (!mData.wasGroundedLastFrame && mData.isGrounded && !mData.isWallRunning
		&& !mData.isSliding) {
		mData.justLanded = true;
		// lastLandingVelocityYは既に上で保存済み
#ifdef _DEBUG
		// デバッグ: 着地検出
		float speed = std::abs(mData.lastLandingVelocityY);
		Console::Print(std::format(
			"[Movement] Landing detected! VelocityY: {:.2f}, Speed: {:.2f}\n",
			mData.lastLandingVelocityY, speed));
#endif
	} else {
		mData.justLanded = false;
	}

	// 前フレームの接地状態を保存
	mData.wasGroundedLastFrame = mData.isGrounded;

	// --- Safety --------------------------------------------------------------
	CheckForNaNAndClamp();
}

void Movement::Ground(float wishspeed, float dt) {
	// Quake/Sourceエンジンの正確な実装：
	// 1. Friction適用
	// 2. 加速（水平速度で計算）
	// 3. 速度を地面にクリップ（1回のみ）

	// 1. Friction
	Friction(dt);

	// 2. 加速（水平方向で計算）
	Vec3 wishdir = mData.wishDirection;
	if (!wishdir.IsZero() && wishspeed > 0.0f) {
		float accel = ConVarManager::GetConVar("sv_accelerate")->
			GetValueAsFloat();
		if (accel > 0.0f) {
			// 水平速度のみで現在速度を計算
			Vec3 vel_horz   = mData.velocity;
			vel_horz.y      = 0;
			const float cur = Math::MtoH(vel_horz).Dot(wishdir);
			const float add = wishspeed - cur;
			if (add > 0.f) {
				float acc = std::min(accel * wishspeed * dt, add);
				mData.velocity += Math::HtoM(acc) * wishdir;
			}
		}
	}

	// 3. 速度を地面にクリップ（地面から離れないように、1回のみ）
	// overbounce=1.001で微小な反発を残す（サーフィンで重要）
	if (mData.velocity.Dot(mData.lastGroundNormal) < 0) {
		mData.velocity = ClipVelocity(mData.velocity, mData.lastGroundNormal,
		                              1.001f);
	}
}

void Movement::Air(float wishspeed, float dt) {
	AirAccelerate(mData.wishDirection, wishspeed,
	              ConVarManager::GetConVar("sv_airaccelerate")->
	              GetValueAsFloat(), dt);
}

void Movement::ApplyHalfGravity(float dt) {
	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	mData.velocity.y -= Math::HtoM(g) * 0.5f * dt;
}

void Movement::Friction(float dt) {
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

	float drop = ctrl * fric * dt;
	float news = std::max(0.0f, speed - drop);
	if (news != speed) {
		news /= speed;
		mData.velocity *= news;
	}
}

void Movement::Accelerate(Vec3 dir, float speed, float accel, float dt) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float cur = Math::MtoH(mData.velocity).Dot(dir);
	const float add = speed - cur;
	if (add <= 0.f) return;
	float acc = std::min(accel * speed * dt, add);
	mData.velocity += Math::HtoM(acc) * dir;
}

void Movement::AirAccelerate(Vec3 dir, float speed, float accel, float dt) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	float       wishspd = std::min(speed, kAirSpeedCap);
	const float cur     = Math::MtoH(mData.velocity).Dot(dir);
	const float add     = wishspd - cur;
	if (add <= 0.f) return;
	float acc = std::min(accel * speed * dt, add);
	mData.velocity += Math::HtoM(acc) * dir;
}

// ----------------------------------------------------------------------------
// Collide & slide (+ step)
// ----------------------------------------------------------------------------
void Movement::MoveCollideAndSlide(float dt) {
	Vec3 vel = mData.velocity;
	Vec3 pos = mScene->GetWorldPos();

	// ---- 地上：ステップ候補と非ステップを比較して滑らかに選択 ----
	if (mData.isGrounded) {
		// まず非ステップ
		Vec3 posNoStep;
		Vec3 velNoStep = vel;
		(void)SlideMove(dt, velNoStep, posNoStep);

		// ステップ（Up→水平→Down）
		Vec3 posStep;
		Vec3 velStep = vel;

		bool stepped = TryStepMove(dt, velStep, posStep);

		// どちらが進んだかで採用（前進量が大きい方）
		Vec3  deltaNo = posNoStep - pos;
		Vec3  deltaSt = (stepped ? (posStep - pos) : Vec3::zero);
		float advNo   = deltaNo.Dot(mData.wishDirection.IsZero() ?
			                            Vec3::zero :
			                            mData.wishDirection);
		float advStep = deltaSt.Dot(mData.wishDirection.IsZero() ?
			                            Vec3::zero :
			                            mData.wishDirection);

		if (stepped && advStep >= advNo) {
			pos = posStep;
			vel = velStep;
		} else {
			pos = posNoStep;
			vel = velNoStep;
		}

		// 最後に「上げない・下だけ」で確定
		StayOnGround(vel, pos);

		mScene->SetWorldPos(pos);
		RefreshHullFromTransform();
		mData.velocity = (vel.SqrLength() < 1e-8f) ? Vec3::zero : vel;
		return;
	}

	// ---- AIR：通常のスライド ----
	Vec3 endPos;
	(void)SlideMove(dt, vel, endPos);
	pos = endPos;

	// 末尾：着地可能なら軽くスナップ（上げない・下だけ）
	{
		Vec3  n;
		float d = 0;
		if (GroundProbe(StepHeightM() + MaxAdhesionM() + RestOffsetM(), n, d) &&
			IsWalkable(n, true)) {
			StayOnGround(vel, pos);
		} else {
			mData.isGrounded = false;
			mData.state      = MOVEMENT_STATE::AIR;
		}
	}

	mScene->SetWorldPos(pos);
	RefreshHullFromTransform();
	mData.velocity = (vel.SqrLength() < 1e-8f) ? Vec3::zero : vel;
}

bool Movement::SlideMove(float dt, Vec3& vel, Vec3& outPos) {
	if (!mUPhysicsEngine) {
		outPos = mScene->GetWorldPos();
		return false;
	}

	auto*       obCvar     = ConVarManager::GetConVar("sv_overbounce");
	const float overbounce = obCvar ?
		                         std::max(obCvar->GetValueAsFloat(), 0.0001f) :
		                         1.0f;

	Vec3  pos      = mScene->GetWorldPos();
	float timeLeft = 1.0f;

	Vec3 planes[kMaxClipPlanes];
	int  numPlanes = 0;

	const float planeEps = 1e-5f, zeroVelEps = 1e-8f, minTimeLeft = 1e-3f;

	for (int bump = 0; bump < kMaxClipPlanes; ++bump) {
		if (vel.SqrLength() < zeroVelEps) {
			vel = Vec3::zero;
			break;
		}

		const Vec3 desired = vel;
		Vec3       move    = vel * dt * timeLeft;
		float      dist    = move.Length();
		if (dist <= 1e-7f) break;
		Vec3 dir = move / dist;

		mScene->SetWorldPos(pos);
		RefreshHullFromTransform();

		UPhysics::Hit hit{};
		if (!mUPhysicsEngine->BoxCast(mData.hull, dir, dist, &hit)) {
			pos += move;
			break;
		}

		if (hit.startSolid) {
			UPhysics::Hit ov{};
			if (mUPhysicsEngine->BoxOverlap(mData.hull, &ov)) {
				// 上向きに押し上げない（水平へずらす or 最小限）
				Vec3 push = ov.normal;
				if (push.y > 0.0f) push.y = 0.0f;
				if (push.SqrLength() < planeEps * planeEps)
					push = Vec3(
						push.x, 0, push.z);
				if (!push.IsZero())
					pos += push.Normalized() * (ov.depth +
						SkinM());
				continue;
			} else {
				vel = Vec3::zero;
				break;
			}
		}

		float travel = std::max(0.f, hit.t * dist - SkinM());
		pos += dir * travel;
		timeLeft *= std::max(0.f, 1.f - hit.t);

		Vec3 n = hit.normal.Normalized();

		// 平面収集（近い法線はまとめる）
		bool stored = false;
		for (int i = 0; i < numPlanes; ++i)
			if (planes[i].Dot(n) > 0.997f) {
				stored = true;
				break;
			}
		if (!stored && numPlanes < kMaxClipPlanes) planes[numPlanes++] = n;

		// 速度クリップ（overbounce）
		vel = ClipVelocity(desired, n, overbounce);

		// 非常に平坦な面でのみy=0にする（サーフィン可能な斜面では垂直速度を保持）
		// 通常の地面（y > 0.7 = 約45度以下）でのみ適用
		if (n.y > 0.7f && mData.isGrounded) vel.y = 0.0f;

		// 多平面矛盾の追加クリップ
		for (int i = 0; i < numPlanes; ++i) {
			if (vel.Dot(planes[i]) < -planeEps)
				vel = ClipVelocity(
					vel, planes[i], 1.0f);
		}

		if (vel.SqrLength() < zeroVelEps) {
			vel = Vec3::zero;
			break;
		}
		if (hit.t >= 1.f - kFracEps || timeLeft <= minTimeLeft) break;
	}

	mScene->SetWorldPos(pos);
	RefreshHullFromTransform();
	outPos = pos;
	return true;
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------
bool Movement::GroundProbe(float len, Vec3& outN, float& outD) {
	if (!mUPhysicsEngine) return false;
	UPhysics::Hit hit{};
	if (mUPhysicsEngine->BoxCast(mData.hull, Vec3::down, len, &hit)) {
		outN = hit.normal;
		outD = hit.t * len;
		return true;
	}
	return false;
}

Vec3 Movement::ClipVelocity(const Vec3& v, const Vec3& n, float overbounce) {
	float back = v.Dot(n);
	if (back > 0.0f) return v; // 離れる向き
	back *= std::max(overbounce, 0.0001f);
	Vec3 out = v - n * back;
	if (out.SqrLength() < 1e-8f) out = Vec3::zero;
	return out;
}

// ----------------------------------------------------------------------------
// Hull & helpers
// ----------------------------------------------------------------------------
void Movement::RefreshHullFromTransform() {
	// 足元原点を前提に、中心を高さの半分だけ上げる
	mData.hull = {
		.center = mScene->GetWorldPos() + Vec3::up * Math::HtoM(
			mData.currentHeight * 0.5f),
		.halfSize = Math::HtoM({
			mData.currentWidth * 0.5f,
			mData.currentHeight * 0.5f,
			mData.currentWidth * 0.5f
		})
	};
}

bool Movement::SweepNoTimeWithSkin(const Vec3& delta, float skinM,
                                   Vec3&       outPos) {
	Vec3  pos = mScene->GetWorldPos();
	float len = delta.Length();
	if (len <= 1e-7f) {
		outPos = pos;
		return true;
	}
	Vec3 dir = delta / len;

	for (int attempt = 0; attempt < 2; ++attempt) {
		mScene->SetWorldPos(pos);
		RefreshHullFromTransform();
		UPhysics::Hit hit{};
		if (!mUPhysicsEngine->BoxCast(mData.hull, dir, len, &hit)) {
			pos += delta;
			break;
		}
		if (hit.startSolid) {
			UPhysics::Hit ov{};
			if (mUPhysicsEngine->BoxOverlap(mData.hull, &ov)) {
				Vec3 push = ov.normal;
				if (push.y > 0.0f) push.y = 0.0f;
				if (push.SqrLength() < 1e-6f) {
					outPos = pos;
					return false;
				}
				pos += push.Normalized() * (ov.depth + CastSkinM());
				continue;
			} else {
				outPos = pos;
				return false;
			}
		}
		float travel = std::max(0.f, hit.t * len - skinM);
		pos += dir * travel;
		break;
	}
	mScene->SetWorldPos(pos);
	RefreshHullFromTransform();
	outPos = pos;
	return true;
}

bool Movement::IsWalkable(const Vec3& n, bool wasGrounded) const {
	const float enterCos = std::cos(kGroundEnterDeg * Math::deg2Rad);
	const float exitCos  = std::cos(kGroundExitDeg * Math::deg2Rad);
	const float ny       = n.y;
	return wasGrounded ? (ny > exitCos) : (ny > enterCos);
}

bool Movement::TryStepMove(float dt, Vec3& ioVel, Vec3& outPos) {
	if (!mUPhysicsEngine) return false;

	const float stepUp   = StepHeightM();
	const Vec3  startPos = mScene->GetWorldPos();
	const Vec3  startVel = ioVel;

	// up
	Vec3 posUp;
	if (!SweepNoTimeWithSkin(Vec3::up * stepUp, CastSkinM(), posUp))
		return
			false;
	mScene->SetWorldPos(posUp);
	RefreshHullFromTransform();

	// horizontal
	Vec3 vel = startVel, posAfter;
	(void)SlideMove(dt, vel, posAfter);

	// down
	Vec3 landedPos = posAfter, landedVel = vel, gN = Vec3::up;
	if (!StepDownToGround(landedPos, landedVel, stepUp, &gN)) {
		// 失敗なら元の位置へ戻す
		mScene->SetWorldPos(startPos);
		RefreshHullFromTransform();
		return false;
	}

	outPos                 = landedPos;
	ioVel                  = landedVel;
	mData.lastGroundNormal = gN;
	mData.lastGroundDistM  = RestOffsetM();
	return true;
}

bool Movement::StepDownToGround(Vec3& ioPos, Vec3& ioVel, float maxDownM,
                                Vec3* outN) {
	mScene->SetWorldPos(ioPos);
	RefreshHullFromTransform();

	Vec3  n;
	float d = 0;
	if (!GroundProbe(maxDownM + RestOffsetM(), n, d)) return false;

	float snap = std::max(0.f, d - RestOffsetM());
	snap       = std::min(snap, maxDownM + MaxAdhesionM());
	if (snap <= 0.f) return false;

	Vec3 snapped;
	if (!SweepNoTimeWithSkin(Vec3::down * snap, RestOffsetM(), snapped))
		return
			false;

	ioPos   = snapped;
	ioVel   = ClipVelocity(ioVel, n, 1.0f);
	ioVel.y = 0.0f;
	if (outN) *outN = n;
	return true;
}

bool Movement::RefreshGroundCache(float probeLenM) {
	Vec3  n;
	float d = 0.0f;
	if (GroundProbe(probeLenM, n, d) && IsWalkable(n, mData.isGrounded)) {
		mData.isGrounded       = true;
		mData.state            = MOVEMENT_STATE::GROUND;
		mData.lastGroundNormal = n;
		mData.lastGroundDistM  = std::max(RestOffsetM(), d);
		mData.hasDoubleJump    = true; // 地上着地でダブルジャンプリセット
		return true;
	}
	mData.isGrounded = false;
	mData.state      = MOVEMENT_STATE::AIR;
	return false;
}

Vec3 Movement::ProjectOntoPlane(const Vec3& v, const Vec3& n) {
	return v - n * v.Dot(n);
}

// 「上げない・下だけ」の StayOnGround
void Movement::StayOnGround(Vec3& ioVel, Vec3& ioPos) {
	if (ioVel.y > kSnapVyMax) return; // 強い上昇中は貼り付けない

	const float liftM         = Math::HtoM(2.0f);
	const float dropM         = StepHeightM() + MaxAdhesionM() + RestOffsetM();
	const float snapTolerance = CastSkinM();

	// 仮想リフト（実座標は上げない）
	Unnamed::Box tmp = mData.hull;
	tmp.center += Vec3::up * liftM;

	UPhysics::Hit hit{};
	if (!mUPhysicsEngine || !mUPhysicsEngine->BoxCast(
		tmp, Vec3::down, dropM + liftM, &hit)) {
		mData.isGrounded = false;
		mData.state      = MOVEMENT_STATE::AIR;
		return;
	}

	Vec3  n = hit.normal.Normalized();
	float d = hit.t * (dropM + liftM) - liftM;
	if (!IsWalkable(n, true)) {
		mData.isGrounded = false;
		mData.state      = MOVEMENT_STATE::AIR;
		return;
	}

	// penetrate が大きいと足元が埋まって見えるため、必要に応じて少し持ち上げる
	const float penetration = RestOffsetM() - d;
	if (penetration > snapTolerance) {
		Vec3 lifted;
		if (SweepNoTimeWithSkin(Vec3::up * penetration, snapTolerance,
		                        lifted)) {
			ioPos = lifted;
			mScene->SetWorldPos(ioPos);
			RefreshHullFromTransform();
			d += penetration;
		}
	}

	float snap = std::max(0.f, d - RestOffsetM());
	if (snap <= snapTolerance) {
		snap = 0.0f;
	} else {
		snap = std::min(snap, dropM);
	}

	if (snap > 0.f) {
		Vec3 snapped;
		if (!SweepNoTimeWithSkin(Vec3::down * snap, RestOffsetM(), snapped)) {
			mData.isGrounded = false;
			mData.state      = MOVEMENT_STATE::AIR;
			return; // スナップに失敗したら接地扱いしない
		}
		ioPos = snapped;
		mScene->SetWorldPos(ioPos);
		RefreshHullFromTransform();
	}

	ioVel   = ClipVelocity(ioVel, n, 1.0f);
	ioVel.y = 0.0f;

	mData.isGrounded       = true;
	mData.state            = MOVEMENT_STATE::GROUND;
	mData.lastGroundNormal = n;
	mData.lastGroundDistM  = std::max(RestOffsetM(), d - snap);
	mData.hasDoubleJump    = true; // 地上着地でダブルジャンプリセット
}


void Movement::CheckForNaNAndClamp() {
	const float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
		GetValueAsFloat();
	for (int i = 0; i < 3; ++i) {
		if (std::isnan(mData.velocity[i])) mData.velocity[i] = 0.0f;
		if (std::isnan(mScene->GetWorldPos()[i])) {
			Vec3 pos = mScene->GetWorldPos();
			pos[i]   = 0.0f;
			mScene->SetWorldPos(pos);
		}
		if (mData.velocity[i] > Math::HtoM(maxVel))
			mData.velocity[i] =
				Math::HtoM(maxVel);
		if (mData.velocity[i] < -Math::HtoM(maxVel))
			mData.velocity[i] = -
				Math::HtoM(maxVel);
	}
}

// ----------------------------------------------------------------------------
// Stuck detection & resolution
// ----------------------------------------------------------------------------
void Movement::DetectAndResolveStuck(float dt) {
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
				RefreshHullFromTransform();

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
				RefreshHullFromTransform();
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

// ----------------------------------------------------------------------------
// Wallrun
// ----------------------------------------------------------------------------
bool Movement::CanWallrun() const {
	// しゃがみボタンが押されている場合はウォールラン不可
	if (mData.wishCrouch) return false;

	// 最小速度があるか
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	float speed   = Math::MtoH(vel_horz.Length());
	if (speed < kWallrunMinSpeed) return false;

	// クールダウン中でないか
	if (mData.timeSinceLastWallRun < kWallrunCooldown) return false;

	// Titanfall 2本家：Wキーなしでも壁に留まる
	// 前進入力があると加速、ないと摩擦で減速

	return true;
}

bool Movement::TryStartWallrun() {
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
	const float checkDistance = Math::HtoM(mData.currentWidth * 0.5f + 10.0f);
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

			// 速度を壁に沿った方向に調整（Titanfall 2スタイル）
			// 本家：カメラの向き（前進方向）に進む
			Vec3 vel_horz      = mData.velocity;
			vel_horz.y         = 0;
			float currentSpeed = vel_horz.Length();

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
			float alongSpeed = vel_horz.Dot(mData.wallRunDirection);
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

			// ウォールラン開始時に小さなブースト（本家の感覚）
			float boostAmount = Math::HtoM(50.0f); // 50 HU/sのブースト
			mData.velocity += mData.wallRunDirection * boostAmount;

			return true;
		}
	}

	return false;
}

void Movement::UpdateWallrun(float dt) {
	mData.wallRunTime += dt;

	// 最大時間を超えたら終了
	if (mData.wallRunTime >= kWallrunMaxTime) {
		EndWallrun();
		return;
	}

	// 壁がまだあるかチェック
	if (mUPhysicsEngine) {
		const float checkDistance = Math::HtoM(
			mData.currentWidth * 0.5f + 20.0f);
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

		// 0.5（約60度）まで許容（以前は0.7=45度で厳しすぎた）
		if (normalDot < 0.5f) {
			EndWallrun();
			return;
		}

		// 法線を徐々に更新（スムーズな遷移）
		mData.wallRunNormal = (mData.wallRunNormal * 0.8f + newNormal * 0.2f).
			Normalized();

		// 壁走り方向も再計算
		if (auto cam = CameraManager::GetActiveCamera()) {
			Vec3 camForward = cam->GetViewMat().Inverse().GetForward();
			camForward.y    = 0;
			if (!camForward.IsZero()) {
				camForward.Normalize();
				Vec3 projectedDir = ProjectOntoPlane(
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
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	if (Math::MtoH(vel_horz.Length()) < kWallrunMinSpeed * 0.5f) {
		EndWallrun();
		return;
	}

	// Titanfall 2本家と同じ：前進入力なしでも続行
	// 左右入力での離脱（オプション）
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
				return;
			}
		}
	}
}

void Movement::EndWallrun() {
	if (!mData.isWallRunning) return;

	mData.isWallRunning        = false;
	mData.state                = MOVEMENT_STATE::AIR;
	mData.lastWallRunNormal    = mData.wallRunNormal;
	mData.timeSinceLastWallRun = 0.0f;
}

void Movement::Wallrun(float wishspeed, float dt) {
	// Wallrun中の動き（Titanfall 2本家の挺動）
	// Wキーで加速、押してないと摩擦で減速

	Vec3 wishdir = mData.wallRunDirection;

	// 前進入力があるか
	if (mData.vecMoveInput.y > 0) {
		// 加速
		float currentSpeed = Math::MtoH(mData.velocity.Dot(wishdir));
		float addSpeed     = wishspeed * 1.2f - currentSpeed;

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
			float fric = ConVarManager::GetConVar("sv_friction")->
				GetValueAsFloat();
			float drop = speed * fric * dt * 0.5f; // 壁では摩擦が弱め
			float news = std::max(0.0f, speed - drop);
			if (news != speed && speed > 0) {
				mData.velocity *= (news / speed);
			}
		}
	}

	// 壁に向かう速度成分を除去
	float intoWall = mData.velocity.Dot(-mData.wallRunNormal);
	if (intoWall > 0) {
		mData.velocity += mData.wallRunNormal * intoWall;
	}

	// 壁に軽く吸い付く力を追加（本家の感覚）
	float pullForce = Math::HtoM(80.0f); // 80 HU/s
	mData.velocity += -mData.wallRunNormal * pullForce * dt;
}

// =============================================================================
// Slide
// =============================================================================

bool Movement::CanSlide() const {
	if (!mData.isGrounded) return false;
	if (!mData.wishCrouch) return false;

	// 水平速度が十分にあるか
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	float speed   = Math::MtoH(vel_horz.Length());

	return speed >= kSlideMinSpeed;
}

void Movement::TryStartSlide() {
	if (!CanSlide()) return;

	// 現在の移動方向をスライディング方向として記録
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;

	if (vel_horz.IsZero()) return;

	mData.slideDirection = vel_horz.Normalized();
	mData.isSliding      = true;
	mData.slideTime      = 0.0f;
	mData.state          = MOVEMENT_STATE::SLIDE;

	// スライディング開始時に小さなブースト
	float currentSpeed = vel_horz.Length();
	float boostedSpeed = currentSpeed + Math::HtoM(kSlideBoostSpeed);

	// 速度キャップを適用（スライドホップの無限加速を防ぐ）
	float speedCapM = Math::HtoM(kSlideHopSpeedCap);
	if (boostedSpeed > speedCapM) {
		boostedSpeed = speedCapM;
	}

	float originalY  = mData.velocity.y;
	mData.velocity   = mData.slideDirection * boostedSpeed;
	mData.velocity.y = originalY; // Y軸は維持

	// ハルサイズをクロウチに変更
	mData.currentHeight = mData.crouchHeight;
}

void Movement::UpdateSlide(float dt) {
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
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	float speed   = Math::MtoH(vel_horz.Length());
	if (speed < kSlideStopSpeed) {
		EndSlide();
		return;
	}

	// クロウチキーが離されたら終了
	if (!mData.wishCrouch) {
		EndSlide();
		return;
	}
}

void Movement::EndSlide() {
	if (!mData.isSliding) return;

	mData.isSliding      = false;
	mData.slideDirection = Vec3::zero;
	mData.slideTime      = 0.0f;

	// ハルサイズを元に戻す
	if (mData.isGrounded) {
		mData.currentHeight = mData.defaultHeight;
		mData.state         = MOVEMENT_STATE::GROUND;
	} else {
		mData.currentHeight = mData.defaultHeight;
		mData.state         = MOVEMENT_STATE::AIR;
	}
}

void Movement::Slide([[maybe_unused]] float wishspeed, float dt) {
	// スライディング中の摩擦（通常より低い）
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	float speed   = Math::MtoH(vel_horz.Length());

	if (speed > 0.1f) {
		// スライディング専用の低摩擦
		float drop     = kSlideFriction * dt;
		float newSpeed = std::max(0.0f, speed - drop);

		if (newSpeed != speed && speed > 0) {
			mData.velocity.x *= (newSpeed / speed);
			mData.velocity.z *= (newSpeed / speed);
		}
	}

	// スライディング方向への入力で少し方向転換可能（Titanfall 2スタイル）
	if (!mData.wishDirection.IsZero()) {
		Vec3 wishDir = mData.wishDirection;
		wishDir.y    = 0;
		if (!wishDir.IsZero()) {
			wishDir.Normalize();

			// 現在の方向とwishDirを少しずつブレンド（10%）
			Vec3 currentDir = vel_horz.Normalized();
			Vec3 newDir = (currentDir * 0.95f + wishDir * 0.05f).Normalized();

			float currentSpeed = vel_horz.Length();
			mData.velocity.x   = newDir.x * currentSpeed;
			mData.velocity.z   = newDir.z * currentSpeed;
		}
	}
}
