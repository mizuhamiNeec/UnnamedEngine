#include "MovementComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/Transform/SceneComponent.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/OldConsole/ConVarManager.h>

#include <engine/Input/InputSystem.h>

void MovementComponent::OnAttach(Entity& owner) { Component::OnAttach(owner); }

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

void MovementComponent::Update(const float dt) {
	ProcessMovement(dt);
}

void MovementComponent::PostPhysics(float) {
}

/// @brief インスペクタ内のImGui描画
void MovementComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("MovementComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("State: %s", ToString(mData.state));
		ImGuiWidgets::DragVec3(
			"Velocity",
			mData.velocity,
			Vec3::zero,
			0.1f,
			"%.3f"
		);
		ImGui::Checkbox("Grounded", &mData.isGrounded);
		ImGui::Text("HeightHU: %.2f  WidthHU: %.2f", mData.currentHeightHu,
		            mData.currentWidthHu);

		if (mData.isWallRunning) {
			ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "ウォールラン! (%.2fs)",
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

Vec3& MovementComponent::GetVelocity() { return mData.velocity; }

Vec3 MovementComponent::GetHeadPos() const {
	// カメラの座標は頭から少し下げた位置
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mData.currentHeightHu - 8.0f);
}

void MovementComponent::SetVelocity(const Vec3& v) { mData.velocity = v; }

bool MovementComponent::IsGrounded() const { return mData.isGrounded; }

bool MovementComponent::WishJump() const { return mData.wishJump; }

bool MovementComponent::IsWallRunning() const { return mData.isWallRunning; }

bool MovementComponent::IsSliding() const { return mData.isSliding; }

bool MovementComponent::HasDoubleJump() const { return mData.hasDoubleJump; }

Vec3 MovementComponent::GetWallRunNormal() const { return mData.wallRunNormal; }

bool MovementComponent::JustLanded() const { return mData.justLanded; }

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

	// 入力方向をカメラ基準にする
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

void MovementComponent::ProcessMovement(const float dt) {
	// 前フレームの接地状態
	mData.wasGroundedLastFrame = mData.isGrounded;

	// しゃがんでいるかで身長を設定
	float targetHU = mData.wishCrouch ?
		                 mData.crouchHeightHu :
		                 mData.defaultHeightHu;
	if (targetHU > mData.currentHeightHu) {
		// 頭上に障害物がないかチェック
		const Vec3         posFeet = mScene->GetWorldPos();
		const Unnamed::Box test    = {
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

	// 高さを再計算
	UpdateHullDimensions();

	// しゃがみ、走り状態で速度を変更
	mData.currentSpeed = mData.wishCrouch ?
		                     mData.crouchSpeed :
		                     mData.sprintSpeed;

	// スライディング
	if (mData.isGrounded && !mData.isSliding && CanSlide()) {
		TryStartSlide();
	}

	// スライディング更新
	if (mData.isSliding) {
		UpdateSlide(dt);
	}

	// ジャンプ
	if (mData.wishJump && mData.isGrounded) {
		mData.velocity.y = Math::HtoM(kJumpVelocityHu);
		mData.isGrounded = false;
		mData.state      = MOVEMENT_STATE::AIR;

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

	// ウォールラン
	mData.timeSinceLastWallRun += dt;
	if (!mData.isGrounded && !mData.isWallRunning && CanWallrun()) {
		TryStartWallrun();
	}

	const bool jumpPressed = mData.wishJump && !mData.lastFrameWishJump;

	if (mData.wishJump) {
		if (mData.isGrounded) {
			// 地上ジャンプ
			mData.velocity.y    = Math::HtoM(kJumpVelocityHu);
			mData.isGrounded    = false;
			mData.state         = MOVEMENT_STATE::AIR;
			mData.hasDoubleJump = true; // 地上ジャンプでダブルジャンプリセット
		} else if (mData.isWallRunning && !mData.wallRunJumpWasPressed) {
			// 現在の進行方向の速度
			Vec3 forwardVel = mData.wallRunDirection * mData.velocity.Dot(
				mData.wallRunDirection);

			// 壁から離れる方向 ( 壁に対して右上に発射 )
			Vec3 awayDir = mData.wallRunNormal * 0.7f + Vec3::up * 1.0f;
			awayDir.Normalize();

			// 進行方向の速度 + 壁から離れるジャンプ
			mData.velocity = forwardVel + awayDir * Math::HtoM(
				kWallrunJumpForce);

			mData.hasDoubleJump = true; // ウォールランジャンプでダブルジャンプリセット
			EndWallrun();
		} else if (!mData.isGrounded && !mData.isWallRunning && mData.
			hasDoubleJump && jumpPressed) {
			// ダブルジャンプ
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

	// --- Movement & collision response (Source-style) ------------------------
	MoveWithCollisions(dt);

	// --- Landing detection (着地直前の速度を保存) ---------------------------
	// 空中で下方向に移動している場合、着地時の速度として保存
	if (!mData.isGrounded && mData.velocity.y < 0.0f) {
		mData.lastLandingVelocityY = mData.velocity.y;
	}

	// --- Stuck detection & resolution ----------------------------------------
	DetectAndResolveStuck(dt);

	// --- Landing detection (着地検出) ----------------------------------------
	// 前フレームで空中、今フレームで地上 = 着地
	if (!mData.wasGroundedLastFrame && mData.isGrounded && !mData.isWallRunning
		&& !mData.isSliding) {
		mData.justLanded = true;
	} else {
		mData.justLanded = false;
	}

	// --- Safety --------------------------------------------------------------
	CheckForNaNAndClamp();
}

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

void MovementComponent::ApplyHalfGravity(float dt) {
	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	mData.velocity.y -= Math::HtoM(g) * 0.5f * dt;
}

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

void MovementComponent::AirAccelerate(
	const Vec3  dir, const float   speed,
	const float accel, const float dt
) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	float       wishspd = std::min(speed, kAirSpeedCap);
	const float cur     = Math::MtoH(mData.velocity).Dot(dir);
	const float add     = wishspd - cur;
	if (add <= 0.f) return;
	float acc = std::min(accel * speed * dt, add);
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
	inline Vec3 ClipVelocity(const Vec3& vel, const Vec3& normal,
	                         float       overbounce) {
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

// ----------------------------------------------------------------------------
// Collision & response (Source-style slide/step and ground snap)
// ----------------------------------------------------------------------------
void MovementComponent::MoveWithCollisions(const float dt) {
	// If no physics engine, move freely
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
				// Free flight
				posFeet += move;
				timeLeft = 0.0f;
				break;
			}

			anyHit = true;

			// Move up to contact (leave a tiny skin)
			const float travel = std::clamp(hit.t, 0.0f, castLen);
			// allowed は「移動分(moveLen)」でクリップする（castLen ではなく）
			float allowed = std::min(moveLen, std::max(0.0f, travel - SkinM()));
			float usedFrac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;

			// 進捗ゼロを許さない（同一点で再ヒットを繰り返して硬直するのを防ぐ）
			if (usedFrac < kFracEps) {
				// もし skin を割っている/ほぼゼロ距離ヒットなら、法線方向に微小デペネ
				if (travel < SkinM()) {
					const float pushOut = (SkinM() - travel) + kTinyAdvanceM;
					posFeet += hit.normal * pushOut;
				}
				// 進捗を必ず作る
				allowed  = std::max(allowed, kTinyAdvanceM);
				usedFrac = std::max(usedFrac, kFracEps);
			}

			posFeet += dir * allowed;
			timeLeft *= (1.0f - usedFrac);

			// Record collision plane (dedupe similar normals)
			Vec3 n = hit.normal;
			if (!n.IsZero()) n.Normalize();
			auto addPlaneUnique = [&](const Vec3& nn) {
				for (int i = 0; i < planeCount; ++i) {
					// ほぼ同じ向きの法線はスキップ
					if (std::fabs(nn.Dot(planes[i])) > kPlaneSameCos) return;
				}
				if (planeCount < kMaxClipPlanes) planes[planeCount++] = nn;
			};
			addPlaneUnique(n);

			// Resolve velocity against accumulated planes (crease support)
			// Start from the original intent for this bump step
			Vec3 primalVel = vel;
			// First, clip against each plane
			for (int i = 0; i < planeCount; ++i) {
				if (primalVel.Dot(planes[i]) < 0.0f) {
					const bool  surfPlane = (planes[i].y < 0.7f); // 非歩行面 ≒ サーフ
					const float over = surfPlane ? kSurfOverclip : kOverclip;
					primalVel = ClipVelocity(primalVel, planes[i], over);
				}
			}
			vel = primalVel;

			// 仕上げ：まだどれかの面に入っているなら、成分を完全消去
			for (int i = 0; i < planeCount; ++i) {
				float into = vel.Dot(planes[i]);
				if (into < 0.0f) {
					// overclip=1.0 の純粋投影で確実に面内へ
					vel = ClipVelocity(vel, planes[i], 1.0f);
				}
			}
			// それでも入っていたら（数値誤差の最終網）：法線成分の減算で強制
			for (int i = 0; i < planeCount; ++i) {
				float into = vel.Dot(planes[i]);
				if (into < 0.0f) {
					vel -= planes[i] * into; // = 平面内へ正確投影
				}
			}

			// If still pushing into any plane, try sliding along crease of two planes
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
					// Trapped by >2 planes: project onto the best crease instead of full stop
					float bestSpeed = -1.0f;
					Vec3  bestDir   = Vec3::zero;
					for (int j = 0; j < planeCount; ++j) {
						for (int k = j + 1; k < planeCount; ++k) {
							Vec3        axis  = planes[j].Cross(planes[k]);
							const float lenSq = axis.SqrLength();
							if (lenSq < 1e-8f) continue;
							axis /= std::sqrt(lenSq);
							float s = std::fabs(vel.Dot(axis));
							if (s > bestSpeed) {
								bestSpeed = s;
								bestDir   = axis;
							}
						}
					}
					if (bestSpeed >= 0.0f) {
						vel = bestDir * bestSpeed; // 稜線に沿って動き継続（ゼロ停止を避ける）
					} else {
						vel = Vec3::zero;
					}
				}
				break;
			}
		}

		return {posFeet, vel, anyHit};
	};

	Vec3 startFeet = mScene->GetWorldPos();

	// Path A: no step
	auto [posA, velA, hitA] = slideMove(startFeet, mData.velocity, dt);

	// Optional step attempt when previously grounded and moving horizontally
	Vec3 posFinal = posA;
	Vec3 velFinal = velA;

	Vec3 horizVel       = mData.velocity;
	horizVel.y          = 0.0f;
	const bool wantStep = mData.wasGroundedLastFrame && (horizVel.SqrLength() >
		1e-8f);
	if (wantStep) {
		// Step up
		Vec3          posUp = startFeet + Vec3::up * StepHeightM();
		Unnamed::Box  boxUp = buildHullAtFeet(posUp);
		UPhysics::Hit ov{};
		const bool    blockedUp = mUPhysicsEngine->BoxOverlap(boxUp, &ov);
		if (!blockedUp) {
			// Move while raised
			auto [posB, velB, hitB] = slideMove(posUp, mData.velocity, dt);

			// Step down
			Unnamed::Box  boxAt = buildHullAtFeet(posB);
			UPhysics::Hit downHit{};
			if (mUPhysicsEngine->BoxCast(boxAt, -Vec3::up,
			                             StepHeightM() + RestOffsetM(),
			                             &downHit)) {
				// Only step down onto walkable slopes (use BoxCast normal)
				const float threshold = 0.7f;
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

	// --- Ground detection (no snap on surf) ------------------------------
	bool isGrounded = false;
	Vec3 posFeet    = posFinal;

	{
		Unnamed::Box  box = buildHullAtFeet(posFeet);
		UPhysics::Hit gHit{};
		const float   snapRange = RestOffsetM() + MaxAdhesionM();

		if (mUPhysicsEngine->BoxCast(box, -Vec3::up, snapRange, &gHit)) {
			// 歩行面閾値（本家相当： ~cos45°）
			const float walkableNY = 0.7f;

			// サーフ(=急斜面)は常に非接地扱い
			const bool isSurf = (gHit.normal.y < walkableNY);

			// 上昇/高速落下中はスナップしない（着地直前の誤スナップ防止）
			const float vy          = mData.velocity.y;
			const bool  allowSnapVy = !(vy > 0.0f) && !(vy < -kSnapVyMax);

			if (!isSurf && allowSnapVy && !(mData.wishJump && vy > 0.0f)) {
				float drop = std::max(0.0f, gHit.t - RestOffsetM());
				posFeet += -Vec3::up * drop;
				isGrounded             = (gHit.normal.y >= walkableNY);
				mData.lastGroundNormal = gHit.normal;
				mData.lastGroundDistM  = gHit.t;
			}
		}
	}

	// Apply
	mScene->SetWorldPos(posFeet);
	mData.velocity   = velFinal;
	mData.isGrounded = isGrounded;
	if (mData.isGrounded && mData.velocity.y < 0.0f) mData.velocity.y = 0.0f;

	if (!mData.isWallRunning && !mData.isSliding) {
		mData.state = mData.isGrounded ?
			              MOVEMENT_STATE::GROUND :
			              MOVEMENT_STATE::AIR;
	}

	UpdateHullDimensions();
}

// ----------------------------------------------------------------------------
// Stuck detection & resolution
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Wallrun
// ----------------------------------------------------------------------------
bool MovementComponent::CanWallrun() const {
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

			// ウォールラン開始
			mData.isWallRunning = true;
			mData.wallRunNormal = wallNormal;
			mData.wallRunTime   = 0.0f;
			mData.state         = MOVEMENT_STATE::WALL_RUN;

			// ウォールラン開始時のジャンプ状態を記録
			mData.wallRunJumpWasPressed = mData.wishJump;

			// ウォールラン開始でダブルジャンプリセット
			mData.hasDoubleJump = true;

			// 速度を壁に沿った方向に調整
			Vec3 vel_horz      = mData.velocity;
			vel_horz.y         = 0;
			float currentSpeed = vel_horz.Length();

			// 壁に沿った方向を計算
			Vec3 along = Vec3::up.Cross(wallNormal).Normalized();

			// カメラ方向と同じ向きに揃える
			if (along.Dot(camForward) < 0) {
				along = -along;
			}
			mData.wallRunDirection = along;

			// 現在の速度を壁に沿った方向に投影
			float alongSpeed = vel_horz.Dot(mData.wallRunDirection);
			if (std::abs(alongSpeed) > 1e-3f) {
				// 壁に沿った方向の速度を使用
				mData.velocity = mData.wallRunDirection * std::abs(alongSpeed);
			} else {
				// 速度がほぼ0の場合、現在の速度のまま
				mData.velocity = mData.wallRunDirection * currentSpeed;
			}

			// 垂直速度の処理
			float originalY = mData.velocity.y;
			if (originalY > 0) {
				mData.velocity.y = originalY * kWallrunVerticalDamping;
			} else if (originalY < 0) {
				// 落下中は軽めに減衰
				mData.velocity.y = originalY * 0.3f;
			}

			// ウォールラン開始時にちょっと加速
			float boostAmount = Math::HtoM(50.0f); // 50 HU/s!
			mData.velocity += mData.wallRunDirection * boostAmount;

			return true;
		}
	}

	return false;
}

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
	Vec3 vel_horz = mData.velocity;
	vel_horz.y    = 0;
	if (Math::MtoH(vel_horz.SqrLength()) < kWallrunMinSpeed) {
		EndWallrun();
		return;
	}

	// カメラ+入力方向を考慮した判定
	// wishDirectionはすでにカメラ方向を考慮した入力方向
	if (!mData.wishDirection.IsZero()) {
		// プレイヤーの入力方向が壁から離れる方向かチェック
		// 壁の法線と入力方向の内積で判定
		float inputToWallDot = mData.wishDirection.Dot(-mData.wallRunNormal);

		// 入力が壁の方向の場合は負の値になる
		// 0より大きい = 壁から離れる方向への入力
		// kWallrunCameraDetachAngle以上壁から離れた方向への入力があれば離脱
		if (inputToWallDot > kWallrunCameraDetachAngle) {
			EndWallrun();
			return;
		}
	}
}

void MovementComponent::EndWallrun() {
	if (!mData.isWallRunning) return;

	mData.isWallRunning        = false;
	mData.state                = MOVEMENT_STATE::AIR;
	mData.lastWallRunNormal    = mData.wallRunNormal;
	mData.timeSinceLastWallRun = 0.0f;
}

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
			const float drop = speed * fric * dt * 0.25f; // 壁では摩擦が弱め
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

	// 壁に吸い付く
	const float pullForce = Math::HtoM(40.0f);
	mData.velocity += -mData.wallRunNormal * pullForce * dt;
}

bool MovementComponent::CanSlide() const {
	if (!mData.isGrounded) return false;
	if (!mData.wishCrouch) return false;

	// 水平速度が十分にあるか
	Vec3 velHorizontal = mData.velocity;
	velHorizontal.y    = 0;
	const float speed  = Math::MtoH(velHorizontal.Length());

	return speed >= kSlideMinSpeed;
}

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
		return;
	}
}

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

void MovementComponent::Slide([[maybe_unused]] float wishspeed, float dt) {
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
