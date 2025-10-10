#include "GameMovementComponent.h"

#include <algorithm>

#include "engine/Camera/CameraManager.h"
#include "engine/Components/Camera/CameraComponent.h"
#include "engine/Components/Transform/SceneComponent.h"
#include "engine/Debug/Debug.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/Input/InputSystem.h"
#include "engine/OldConsole/ConVarManager.h"
#include "engine/uphysics/PhysicsTypes.h"
#include "runtime/physics/core/UPhysics.h"

GameMovementComponent::~GameMovementComponent() = default;

void GameMovementComponent::SetUPhysicsEngine(UPhysics::Engine* engine) {
	mUPhysicsEngine = engine;
}

Vec3 GameMovementComponent::GetHeadPos() const {
	return mScene->GetWorldPos() + Vec3::up * Math::HtoM(
		mCurrentHeightHu - 9.0f);
}

Vec3 GameMovementComponent::GetVelocity() const {
	return mVecVelocity;
}

void GameMovementComponent::SetVelocity(const Vec3 newVel) {
	mVecVelocity = newVel;
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

	mJustAutoJumped = false;

	if (mUngroundTimer > 0.0f) {
		mUngroundTimer = std::max<float>(0.0f, mUngroundTimer - mDeltaTime);
	}

	if (!mIsGrounded) {
		ApplyHalfGravity();
	}

	const auto ground = CheckGrounded();
	mWasGrounded ?
		mSinceLanded += mDeltaTime :
		mSinceLanded = 0.0f;

	const bool wasGrounded = mWasGrounded;
	mIsGrounded = ground.onGround;

	const bool justLanded = !wasGrounded && mIsGrounded;

	if (mJumpBufferTimer > 0.0f) {
		mJumpBufferTimer = std::max<float>(0.0f, mJumpBufferTimer - mDeltaTime);
	}

	if (mPressedJumpThisFrame && mIsGrounded) {
		Jump();
	}
	else if (
		mAutoBHopEnabled &&
		justLanded &&
		(mWishJump || mJumpBufferTimer > 0.0f)
		) {
		Jump();
		mJustAutoJumped = true;
		mJumpBufferTimer = 0.0f;
	}

	//const bool skipGroundFriction = mJustAutoJumped ||
	//	(mIsGrounded && mSinceLanded <= kBHopFrictionSkipSec);

	Vec3 wishDir = mVecWishVel;
	if (mIsGrounded) {
		wishDir = ProjectOntoPlane(wishDir, ground.normal);
		if (!wishDir.IsZero()) wishDir.Normalize();
	}

	// ★ 地上での摩擦スキップ条件（オートBHop/着地フレーム）
	const bool skipGroundFriction =
		mIsGrounded && (mJustAutoJumped || (mAutoBHopEnabled && mWishJump));

	// 地上の法線成分をまず消して（床接線へ拘束）
	if (mIsGrounded) {
		if (!skipGroundFriction) {
			Friction(); // ← 既存
		}
		mVecVelocity -= ground.normal * mVecVelocity.Dot(ground.normal);
	}

	// ★ ここを「地上なら常に Ground 加速」に変更
	float wishspeed = mVecWishVel.Length() * mSpeed;
	float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
	if (wishspeed > maxspeed) {
		wishspeed = maxspeed; // dir をスケールせず、speed 側をクランプ
	}

	if (mIsGrounded) {
		// 地上：床平面に沿った方向で加速（失速しない）
		Accelerate(
			wishDir, wishspeed,
			ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat()
		);
		// スナップは移動解決後に適用（既存のまま）
	}
	else {
		// 空中のみ AirAccelerate（30HU/s クランプは空中だけに適用される）
		AirAccelerate(
			mVecWishVel, wishspeed,
			ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat()
		);
		ApplyHalfGravity();
	}

	if (kBHopSpeedCap > 0.0f) {
		Vec3 flat = mVecVelocity;
		flat.y = 0.0f;
		float s = flat.Length();
		if (s > kBHopSpeedCap) {
			flat *= (kBHopSpeedCap / s);
			mVecVelocity.x = flat.x;
			mVecVelocity.z = flat.z;
		}
	}

	const Vec3 startPos = mScene->GetWorldPos();
	const Vec3 startVel = mVecVelocity;

	const Unnamed::Box box{
		startPos + Vec3::up * Math::HtoM(mCurrentHeightHu * 0.5f),
		{
			Math::HtoM(mCurrentWidthHu * 0.5f),
			Math::HtoM(mCurrentHeightHu * 0.5f),
			Math::HtoM(mCurrentWidthHu * 0.5f)
		}
	};
	const auto result =
		SlideMove(startPos, startVel, box, mDeltaTime, Vec3::up);

	// 移動
	mScene->SetWorldPos(result.endPos);
	mVecVelocity = result.endVel;

	// 「最終位置」決定後にスナップ補正を適用して、見た目の浮きを解消
	if (result.hitGround) {
		mScene->SetWorldPos(mScene->GetWorldPos() + ConsumeSnapOffset());
	}

	mWasGrounded = result.hitGround;
	if (result.hitGround) {
		mPrevGroundNormal = result.groundNormal;
	}
	CheckVelocity();
}

void GameMovementComponent::PostPhysics(float) {
	Debug::DrawArrow(
		mScene->GetWorldPos(),
		mVecVelocity * 0.25f,
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
		ImGuiWidgets::DragVec3(
			"velocity",
			mVecVelocity,
			Vec3::zero,
			0.1f,
			"%.2f m/s"
		);

		ImGui::Checkbox("onGround", &mIsGrounded);
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
	mVecMoveInput = Vec3::zero;

	if (InputSystem::IsPressed("forward")) {
		mVecMoveInput.z += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		mVecMoveInput.z -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		mVecMoveInput.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		mVecMoveInput.x -= 1.0f;
	}

	mVecMoveInput.Normalize();

	const bool jumpDown = InputSystem::IsPressed("jump");
	mPressedJumpThisFrame = jumpDown && !mPrevJumpDown;
	mPrevJumpDown = jumpDown;
	mWishJump = jumpDown;

	mWishJump = jumpDown;
	if (mPressedJumpThisFrame && !mIsGrounded) {
		mJumpBufferTimer = kJumpBufferTimeSec;
	}

	if (InputSystem::IsPressed("crouch")) {
		mWishCrouch = true;
	}

	if (InputSystem::IsReleased("crouch")) {
		mWishCrouch = false;
	}
}

void GameMovementComponent::CalcMoveDir() {
	const auto camera = CameraManager::GetActiveCamera();
	Vec3       cameraForward = camera->GetViewMat().Inverse().GetForward();

	cameraForward.y = 0.0f;
	cameraForward.Normalize();

	Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

	mVecWishVel = cameraForward * mVecMoveInput.z + cameraRight * mVecMoveInput.
		x;
	mVecWishVel.y = 0.0f;

	if (!mVecWishVel.IsZero()) {
		mVecWishVel.Normalize();
	}
}

Vec3 GameMovementComponent::BlendNormal(
	const Vec3& now, const Vec3& prev,
	const float blendHz, const float deltaTime
) {
	const float a = std::clamp(blendHz * deltaTime, 0.0f, 1.0f);
	return (prev * (1.0f - a) + now * a).Normalized();
}

Vec3 GameMovementComponent::ConsumeSnapOffset() {
	const Vec3 o = mSnapOffset;
	mSnapOffset = Vec3::zero;
	return o;
}

GameMovementComponent::GroundContact GameMovementComponent::CheckGrounded() {
	constexpr float kStepHU = 18.0f;
	constexpr float kEpsHU = 2.0f;
	constexpr float kMaxCCDDownHu = 64.0f;
	constexpr float kCoyoteTime = 0.25f;
	constexpr float kNormalBlendHz = 50.0f;

	GroundContact result;
	if (!mUPhysicsEngine) {
		Warning("GameMovement", "Physics engine is null.");
		return result;
	}

	// 上昇中/Unground中は接地しない（ジャンプ直後誤接地を防ぐ）
	if (mUngroundTimer > 0.0f || mVecVelocity.Dot(Vec3::up) > 0.0f) {
		mTimeSinceLost += mDeltaTime;
		result.onGround = false;
		result.normal = Vec3::up;
		result.distance = 0.0f;
		mSnapOffset = Vec3::zero;
		return result;
	}

	const float stepM = Math::HtoM(kStepHU);
	const float epsM = Math::HtoM(kEpsHU);
	const float maxCCDDownM = Math::HtoM(kMaxCCDDownHu);

	float extraDown = std::max<float>(
		0.0f, -mVecVelocity.Dot(Vec3::up) * mDeltaTime);
	extraDown = std::min<float>(extraDown, maxCCDDownM);

	// 足元から step 分上
	const Vec3 feetPos = mScene->GetWorldPos();
	const Vec3 start = feetPos + Vec3::up * stepM;

	// ★ Z も幅/2 に修正
	Unnamed::Box castBox{
		.center = start,
		.halfSize = {
			Math::HtoM(mCurrentWidthHu * 0.5f), // X
			stepM,                              // Y（短い柱）
			Math::HtoM(mCurrentWidthHu * 0.5f)  // Z ← ここを修正
		}
	};

	const float length = stepM + epsM + extraDown;

	UPhysics::Hit hit{};
	const bool    anyHit = mUPhysicsEngine->BoxCast(
		castBox, Vec3::down, length, &hit);
	if (anyHit) {
		const float nUp = hit.normal.Dot(Vec3::up);
		const bool  walkable = (nUp >= std::cos(Math::deg2Rad * kMaxSlopeDeg));
		if (walkable) {
			result.onGround = true;
			result.normal = BlendNormal(hit.normal, mPrevGroundNormal,
				kNormalBlendHz, mDeltaTime);

			// ★ 距離は “正規化距離” で統一（t/fraction は必ずエンジンに合わせて！）
			const float frac = std::clamp(hit.t, 0.0f, 1.0f);
			// ← HitInfoがtならhit.tに
			const float traveled = frac * length; // start→ヒットまでの実移動

			const float deadZoneM = Math::HtoM(0.5f);   // 0.5HU 程度
			const float signedSnap = (stepM - traveled); // +: 上へ, −: 下へ

			Vec3 snap = Vec3::zero;
			if (signedSnap > deadZoneM) {
				// 貫入している → 上へ戻す
				snap = Vec3::up * (signedSnap - deadZoneM);
			}
			else if (signedSnap < -deadZoneM) {
				// 浮いている → 下へ寄せる
				snap = -Vec3::up * (-signedSnap - deadZoneM);
			}
			mSnapOffset = snap;

			mTimeSinceLost = 0.0f;
			mPrevGroundNormal = result.normal;
			return result;
		}
	}

	// コヨーテ（落下時の見失い救済のみ）
	mTimeSinceLost += mDeltaTime;
	if (mTimeSinceLost < kCoyoteTime && mVecVelocity.y <= 0.0f) {
		result.onGround = true;
		result.normal = mPrevGroundNormal;
		result.distance = 0.0f;
		mSnapOffset = Vec3::zero;
		return result;
	}

	result.onGround = false;
	result.normal = Vec3::up;
	result.distance = 0.0f;
	mSnapOffset = Vec3::zero;
	return result;
}

Vec3 GameMovementComponent::ClipVelocityToPlane(
	const Vec3& v, const Vec3& n, float overBounce
) {
	// Quake/Source の PM_ClipVelocity 相当
	const float backoff = v.Dot(n) * overBounce;
	Vec3        out = v - n * backoff;
	return out;
}

bool GameMovementComponent::IsWalkable(
	const Vec3& n, const Vec3& up, float maxSlopeDeg
) {
	return n.Normalized().Dot(up.Normalized()) >= MinGroundDot(maxSlopeDeg);
}

float GameMovementComponent::MinGroundDot(const float maxSlopeDeg) {
	return std::cos(Math::deg2Rad * maxSlopeDeg);
}

bool GameMovementComponent::IsCeiling(
	const Vec3& n, const Vec3& up, float maxSlopeDeg
) {
	return n.Normalized().Dot(up) <= -MinGroundDot(maxSlopeDeg);
}

Vec3 GameMovementComponent::ProjectOntoPlane(const Vec3& v, const Vec3& n) {
	return v - n * v.Dot(n);
}

GameMovementComponent::MoveResult GameMovementComponent::SlideMove(
	const Vec3           startPos,
	const Vec3           startVel,
	const Unnamed::Box& shape,
	const float          dt,
	const Vec3& up
) {
	constexpr float kTiny = 1e-6f;

	MoveResult mr{};
	Vec3       pos = startPos;
	Vec3       vel = startVel;

	// ★ 常に「足元＋半身高」に Box.center を置く（前回の修正を維持）
	const float   halfHeightM = shape.halfSize.y;
	Unnamed::Box box = shape;
	box.center = pos + up * halfHeightM;

	float timeLeft = dt;

	Vec3 planes[8];
	int  numPlanes = 0;
	bool touchedGround = false;
	bool touchedCeiling = false;
	Vec3 groundN = up;

	for (int bump = 0; bump < kMaxBumps && timeLeft > 0.0f; ++bump) {
		const Vec3 wishDelta = vel * timeLeft;
		if (wishDelta.SqrLength() < kTiny) break;

		UPhysics::Hit hit{};
		Vec3          dir = wishDelta;
		const float   len = dir.Length();
		dir.Normalize();
		if (!mUPhysicsEngine->BoxCast(box, dir, len, &hit)) {
			pos += wishDelta;
			box.center = pos + up * halfHeightM;
			break;
		}

		mr.hitSomething = true;

		const float frac = std::clamp(hit.t, 0.0f, 1.0f);
		const float safe = std::max<float>(0.0f, frac - 1e-4f);

		if (safe <= 1e-4f) {
			const float kUnstickEpsM = Math::HtoM(kUnstickEpsHU); // ← 綴り統一
			const float kPushOutEpsM = Math::HtoM(kPushOutEpsHU); // ← 綴り統一

			// まずは “今回のヒット法線 n” を基本の押し出し方向にする
			Vec3 pushN = hit.normal.Normalized();
			// ← 直前の hit.normal.Normalized() を使う

			// Overlap 正常なら normal を上書き採用（使える実装なら）
			UPhysics::Hit overlap{};
			if (mUPhysicsEngine->BoxOverlap(box, &overlap) && overlap.normal.
				SqrLength() > 1e-9f) {
				pushN = overlap.normal.Normalized();
			}

			if (touchedGround) {
				// 地面平面で“壁から離れる”方向にずらす
				Vec3 away2D = ProjectOntoPlane(pushN, groundN);
				if (away2D.SqrLength() > 1e-9f) {
					away2D.Normalize();
					pos += away2D * kUnstickEpsM;
				}
				else {
					// 押し出し方向が垂直に近い場合は少し下げて再試行
					pos -= up * kUnstickEpsM;
				}
			}
			else {
				// 空中は単純に法線方向へ
				pos += pushN * kPushOutEpsM;
			}

			box.center = pos + up * halfHeightM;

			// 同一点ループ回避のため僅かに時間を消費
			timeLeft *= 0.95f;

			// 接地中の上向き禁止
			if (touchedGround && vel.Dot(up) > 0.0f) {
				vel -= up * vel.Dot(up);
			}
			continue;
		}

		pos += wishDelta * safe;
		box.center = pos + up * halfHeightM;

		// ★時間比で減らす
		timeLeft *= (1.0f - safe);

		Vec3       n = hit.normal.Normalized();
		const bool onG = IsWalkable(n, up, kMaxSlopeDeg);
		const bool onC = IsCeiling(n, up, kMaxSlopeDeg);
		if (onG) {
			touchedGround = true;
			groundN = n;
		}
		if (onC) { touchedCeiling = true; }

		// --- 天井特別扱い（床に触れている間） --------------------------
		if (touchedGround && onC) {
			// 天井法線を地面平面へ投影→水平成分のみでクリップ
			Vec3        ceilFlatN = ProjectOntoPlane(n, up);
			const float l2 = ceilFlatN.SqrLength();
			if (l2 > kTiny) {
				ceilFlatN *= 1.0f / std::sqrt(l2);
				Vec3 velFlat = ProjectOntoPlane(vel, up);
				velFlat = ClipVelocityToPlane(velFlat, ceilFlatN, 1.0f);
				// 上向きは禁止
				const float vyUp = std::max<float>(0.0f, vel.Dot(up));
				vel = velFlat - up * vyUp;
			}
			else {
				vel -= up * std::max<float>(0.0f, vel.Dot(up));
			}
			ceilFlatN = ProjectOntoPlane(n, up).Normalized();
			Vec3 edge = groundN.Cross(ceilFlatN);
			if (edge.SqrLength() > 1e-9f) {
				edge.Normalize();
				float along = vel.Dot(edge);
				vel = edge * along;
			}
			// 上向き禁止（既存のまま）
			const float vyUp = std::max<float>(0.0f, vel.Dot(up));
			vel -= up * vyUp;
			continue;
		}
		// ---------------------------------------------------------------

		// 通常のクリップ（床/壁）
		vel = ClipVelocityToPlane(vel, n, kOverBounce);

		// 多面処理（★天井は入ってこない）
		if (numPlanes < kMaxPlanes) planes[numPlanes++] = n;
		for (int i = 0; i < numPlanes - 1; ++i) {
			if (vel.Dot(planes[i]) < 0.0f) {
				Vec3        edge = planes[i].Cross(planes[numPlanes - 1]);
				const float l2 = edge.SqrLength();
				if (l2 > kTiny) {
					edge *= 1.0f / std::sqrt(l2);
					vel = edge * vel.Dot(edge);
				}
				else {
					vel = Vec3::zero;
					break;
				}
			}
		}
		if (vel.SqrLength() < kTiny) {
			vel = Vec3::zero;
			break;
		}
	}

	// ★床優先：最終速度は地面平面へ射影（ランプブースト抑制にも効く）
	if (touchedGround) {
		vel = ProjectOntoPlane(vel, groundN);
	}

	// ★床＋天井を同時に触ったフレームでは、上向き最終禁止
	if (touchedGround && touchedCeiling && vel.Dot(up) > 0.0f) {
		vel -= up * vel.Dot(up);
	}

	UPhysics::Hit probe{};
	if (mUPhysicsEngine->BoxOverlap(box, &probe)) {
		if (touchedGround) {
			pos -= up * Math::HtoM(0.5f); // 0.5 HU 下げる
		}
		else if (probe.normal.SqrLength() > 1e-9f) {
			pos += probe.normal.Normalized() * Math::HtoM(0.5f);
		}
		box.center = pos + up * halfHeightM;
		mr.endPos = pos;
	}

	mr.endPos = pos;
	mr.endVel = vel;
	mr.hitGround = touchedGround;
	mr.groundNormal = groundN;
	return mr;
}

void GameMovementComponent::Jump() {
	mVecVelocity.y = Math::HtoM(mJumpVel);

	mIsGrounded = false;
	mUngroundTimer = kJumpUngroundTime;

	mSinceLanded = kBHopFrictionSkipSec * 2.0f;

	DevMsg(
		"GameMovement",
		"Jump fired (auto={})",
		mJustAutoJumped ? 1 : 0
	);
}

void GameMovementComponent::ApplyHalfGravity() {
	const float gravity = ConVarManager::GetConVar("sv_gravity")->
		GetValueAsFloat();
	mVecVelocity.y -= Math::HtoM(gravity) * 0.5f * mDeltaTime;
}

void GameMovementComponent::Friction() {
	Vec3 flat = mVecVelocity;
	flat.y = 0.0f;

	float speed = flat.Length();
	if (speed < 0.1f) {
		return;
	}

	const float friction = ConVarManager::GetConVar("sv_friction")->
		GetValueAsFloat();
	const float stopspeed_m = Math::HtoM(
		ConVarManager::GetConVar("sv_stopspeed")->GetValueAsFloat());

	const float control = std::max<float>(stopspeed_m, speed);
	const float drop = control * friction * mDeltaTime;

	const float newspeed = std::max<float>(0.0f, speed - drop);
	if (newspeed != speed) {
		const float scale = newspeed / speed;
		// 水平成分のみ減衰（垂直成分には摩擦をかけない）
		mVecVelocity.x *= scale;
		mVecVelocity.z *= scale;
	}
}

void GameMovementComponent::Accelerate(
	const Vec3& dir, float speed, float accel
) {
	float currentSpeed = Math::MtoH(mVecVelocity).Dot(dir);
	float addspeed = speed - currentSpeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed = std::min<float>(accelspeed, addspeed);
	mVecVelocity += Math::HtoM(accelspeed) * dir;
}

void GameMovementComponent::AirAccelerate(
	const Vec3& dir, float speed, float accel
) {
	float wishspd = speed;

	wishspd = std::min<float>(wishspd, 30.0f);
	float currentspeed = Math::MtoH(mVecVelocity).Dot(dir);
	float addspeed = wishspd - currentspeed;

	if (addspeed <= 0.0f) {
		return;
	}

	float accelspeed = accel * mDeltaTime * speed;
	accelspeed = std::min<float>(accelspeed, addspeed);
	mVecVelocity += Math::HtoM(accelspeed) * dir;
}

void GameMovementComponent::CheckVelocity() {
	for (int i = 0; i < 3; ++i) {
		std::string name = ConVarManager::GetConVar("name")->GetValueAsString();
		const float maxVel = ConVarManager::GetConVar("sv_maxvelocity")->
			GetValueAsFloat();

		if (isnan(mVecVelocity[i])) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a NaN velocity {}",
				name, StrUtil::DescribeAxis(i)
			);
			mVecVelocity[i] = 0.0f;
		}

		if (isnan(mScene->GetWorldPos()[i])) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a NaN position on {}",
				name, StrUtil::DescribeAxis(i)
			);
			Vec3 pos = mScene->GetWorldPos();
			pos[i] = 0.0f;
			mScene->SetWorldPos(pos);
		}

		if (mVecVelocity[i] > Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too high on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mVecVelocity[i] = Math::HtoM(maxVel);
		}
		else if (mVecVelocity[i] < -Math::HtoM(maxVel)) {
			DevMsg(
				"GameMovementComponent",
				"{}  Got a velocity too low on {}",
				name, StrUtil::DescribeAxis(i)
			);
			mVecVelocity[i] = -Math::HtoM(maxVel);
		}
	}
}
