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
	mUPhysicsEngine  = uphysics;
	mData            = md;
	mData.velocity   = Vec3::zero;
	mData.state      = MOVEMENT_STATE::AIR;
	mData.isGrounded = false;
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

	// --- Jump ---------------------------------------------------------------
	if (mData.wishJump && mData.isGrounded) {
		mData.velocity.y = Math::HtoM(kJumpVelocityHu);
		mData.isGrounded = false;
		mData.state      = MOVEMENT_STATE::AIR;
	}

	const float wishspeed = mData.wishDirection.IsZero() ?
		                        0.0f :
		                        mData.currentSpeed;

	// --- Accelerations & gravity --------------------------------------------
	if (!mData.isGrounded) ApplyHalfGravity(dt);
	if (mData.isGrounded) Ground(wishspeed, dt);
	else Air(wishspeed, dt);
	if (!mData.isGrounded) ApplyHalfGravity(dt);

	// --- Collision & stepping -----------------------------------------------
	MoveCollideAndSlide(dt);

	// --- Safety --------------------------------------------------------------
	CheckForNaNAndClamp();
}

void Movement::Ground(float wishspeed, float dt) {
	// Quake/Sourceエンジンの正確な実装：
	// 1. 速度を地面にクリップ
	// 2. Friction適用
	// 3. 加速（水平速度で計算）
	// 4. 再度クリップ
	
	// 1. 速度を地面にクリップ（常に実行）
	mData.velocity = ClipVelocity(mData.velocity, mData.lastGroundNormal, 1.0f);
	
	// 2. Friction
	Friction(dt);

	// 3. 加速（水平方向で計算）
	Vec3 wishdir = mData.wishDirection;
	if (!wishdir.IsZero() && wishspeed > 0.0f) {
		float accel = ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat();
		if (accel > 0.0f) {
			// 水平速度のみで現在速度を計算
			Vec3 vel_horz = mData.velocity;
			vel_horz.y = 0;
			const float cur = Math::MtoH(vel_horz).Dot(wishdir);
			const float add = wishspeed - cur;
			if (add > 0.f) {
				float acc = std::min(accel * wishspeed * dt, add);
				mData.velocity += Math::HtoM(acc) * wishdir;
			}
		}
	}
	
	// 4. 速度を再度地面にクリップ（地面から離れないように）
	mData.velocity = ClipVelocity(mData.velocity, mData.lastGroundNormal, 1.0f);
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
	Vec3 vel_horz = mData.velocity;
	vel_horz.y = 0;
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

		// Walkable なら y=0 固定（登りは位置で表現）
		if (IsWalkable(n, mData.isGrounded)) vel.y = 0.0f;

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
