#include <pch.h>

#include <algorithm>
#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Components/Camera/CameraComponent.h>
#include <engine/public/Components/ColliderComponent/BoxColliderComponent.h>
#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/Entity/Entity.h>
#include <engine/public/ImGui/ImGuiWidgets.h>
#include <engine/public/Input/InputSystem.h>
#include <engine/public/OldConsole/Console.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/Physics/PhysicsEngine.h>
#include <engine/public/uphysics/UPhysics.h>
#include <engine/public/uphysics/Primitives.h>
#include <game/public/components/PlayerMovementUPhysics.h>

class ColliderComponent;

PlayerMovementUPhysics::~PlayerMovementUPhysics() {
}

void PlayerMovementUPhysics::OnAttach(Entity& owner) {
	CharacterMovement::OnAttach(owner);

	// 初期化処理
	// トランスフォームコンポーネントを取得
	mScene = mOwner->GetTransform();

	mSpeed   = 290.0f;
	mJumpVel = 350.0f;

	// Console::SubmitCommand("sv_gravity 0");
}

void PlayerMovementUPhysics::ProcessInput() {
	//-------------------------------------------------------------------------
	// 移動入力
	//-------------------------------------------------------------------------
	mMoveInput = {0.0f, 0.0f, 0.0f};

	if (InputSystem::IsPressed("forward")) {
		mMoveInput.z += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		mMoveInput.z -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		mMoveInput.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		mMoveInput.x -= 1.0f;
	}

	if (InputSystem::IsPressed("crouch")) {
		mWishCrouch = true;
	}

	if (InputSystem::IsReleased("crouch")) {
		mWishCrouch = false;
	}

	mMoveInput.Normalize();
}

void PlayerMovementUPhysics::Update([[maybe_unused]] const float deltaTime) {
	mDeltaTime = deltaTime;
	mPosition  = mScene->GetLocalPos();

	// 入力処理
	ProcessInput();

	// 前回状態を保存
	mWasGrounded        = mIsGrounded;
	mPreviousSlideState = mSlideState;

	if (mWishCrouch && !mIsCrouching) {
		mIsCrouching     = true;
		mCurrentHeightHu = kDefaultHeightHU * 0.5f;
	} else if (!mWishCrouch && mIsCrouching) {
		mIsCrouching     = false;
		mCurrentHeightHu = kDefaultHeightHU;
	}

	Vec3 oldWorldPos = mScene->GetWorldPos();
	CollideAndSlide(mVelocity * mDeltaTime);

	Move();

	// スライディング開始の検出とイベント実行
	if (mSlideState == Sliding && mPreviousSlideState != Sliding) {
		// スライディング開始時のカメラシェイク（初回のみ）
		StartCameraShake(0.4f, 0.03f, 5.0f, Vec3::forward, 0.015f, 3.5f,
		                 Vec3::right);
	}

	UpdateCameraShake(mDeltaTime);

	// デバッグ描画
	Debug::DrawArrow(mScene->GetWorldPos(), mVelocity, Vec4::yellow);

	const float width  = Math::HtoM(mCurrentWidthHu);
	const float height = Math::HtoM(mCurrentHeightHu);
	Debug::DrawBox(
		mScene->GetWorldPos() + (Vec3::up * height * 0.5f),
		Quaternion::Euler(Vec3::zero),
		Vec3(width, height, width),
		mIsGrounded ? Vec4::green : Vec4::blue);
	Debug::DrawRay(mScene->GetWorldPos(), mWishdir, Vec4::cyan);

	mScene->SetLocalPos(mPosition);

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera        = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();

	auto* collider = mOwner->GetComponent<ColliderComponent>();

	if (!collider) {
		Console::Print(
			"PlayerMovement::CollideAndSlide() : ColliderComponent is not attached to the owner entity.",
			Vec4::yellow,
			Channel::Physics
		);
	} else {
		UPhysics::Ray ray;
		ray.start  = camera->GetViewMat().Inverse().GetTranslate();
		ray.dir    = cameraForward;
		ray.invDir = Vec3(
			cameraForward.x != 0.0f ? 1.0f / cameraForward.x : FLT_MAX,
			cameraForward.y != 0.0f ? 1.0f / cameraForward.y : FLT_MAX,
			cameraForward.z != 0.0f ? 1.0f / cameraForward.z : FLT_MAX
		);
		ray.tMin = 0.0f;
		ray.tMax = 100.0f;

		UPhysics::Hit hit;
		bool          isHit = mUPhysicsEngine->RayCast(ray, &hit);

		Debug::DrawRay(
			camera->GetViewMat().Inverse().GetTranslate() + camera->GetViewMat()
			.Inverse().GetForward() * 2.0f, cameraForward * 98.0f, Vec4::red);
		if (isHit) {
			Debug::DrawBox(hit.pos, Quaternion::identity,
			               Vec3(0.1f, 0.1f, 0.1f), Vec4::green);
			Debug::DrawAxis(hit.pos, Quaternion::identity);
			Debug::DrawRay(hit.pos, hit.normal, Vec4::magenta);
		}
	}
}

static float kEdgeAngleThreshold = 0.2f; // エッジ判定の閾値

void PlayerMovementUPhysics::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("PlayerMovement",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGuiWidgets::DragVec3(
			"Velocity",
			mVelocity,
			Vec3::zero,
			0.1f,
			"%.2f m/s"
		);
		ImGui::DragFloat("edge angle threshold", &kEdgeAngleThreshold, 0.01f,
		                 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("State Info:");
		ImGui::Text("IsGrounded: %s", mIsGrounded ? "Yes" : "No");
		ImGui::Text("WasGrounded: %s", mWasGrounded ? "Yes" : "No");

		const char* slideStateStr =
			mSlideState == NotSliding
				? "Not Sliding"
				: (mSlideState == Sliding ? "Sliding" : "Recovery");

		const char* prevSlideStateStr =
			mPreviousSlideState == NotSliding
				? "Not Sliding"
				: (mPreviousSlideState == Sliding ? "Sliding" : "Recovery");

		ImGui::Text("Slide State: %s (Prev: %s)", slideStateStr,
		            prevSlideStateStr);
		ImGui::Text("State Changed: %s",
		            (mSlideState != mPreviousSlideState) ? "Yes" : "No");

		// 現在適用されている摩擦値の表示
		float currentFriction = mSlideState == Sliding
			                        ? mSlideFriction
			                        : ConVarManager::GetConVar("sv_friction")->
			                        GetValueAsFloat();
		ImGui::Text("Current Friction: %.4f", currentFriction);
	}
#endif
}


void PlayerMovementUPhysics::Move() {
	// 前回のフレームの接地状態を保存
	mWasGrounded        = mIsGrounded;
	mPreviousSlideState = mSlideState;

	if (!mIsGrounded) {
		ApplyHalfGravity();
	}

	//mIsGrounded = CheckGrounded();

	if (!mWasGrounded && mIsGrounded) {
		// 着地処理

		// スライド状態でない場合のみ通常摩擦に設定
		if (mSlideState != Sliding) {
			// 着地時の衝撃が大きい場合のみカメラシェイク
			if (mVelocity.y < -Math::HtoM(200.0f)) {
				float landingForce = -mVelocity.y / Math::HtoM(350.0f);
				// 正規化された着地の強さ
				StartCameraShake(0.2f, landingForce * 0.05f, 10.0f, Vec3::down,
				                 landingForce * 0.02f, 8.0f, Vec3::forward);
			}
		}
	}

	// スライディング開始条件チェック
	if (mIsGrounded && mWishCrouch && mSlideState
		==
		NotSliding
	) {
		float currentSpeed = Math::MtoH(mVelocity).Length();
		if (currentSpeed > mSlideMinSpeed) {
			// スライディング開始
			mSlideState = Sliding;
			mSlideTimer = 0.0f;

			// しゃがみ状態設定
			mIsCrouching     = true;
			mCurrentHeightHu = kDefaultHeightHU * 0.5f;

			// スライド方向を現在の速度方向に設定
			mSlideDir = mVelocity.Normalized();

			// 前方向に加速
			mVelocity += mSlideDir * mSlideBoost;

			// 下方向への力を加えて斜面を下りやすくする
			mVelocity.y -= Math::HtoM(mSlideDownForce) * mDeltaTime;
		}
	} else
	// スライディング中の処理
		if (mSlideState == Sliding) {
			mSlideTimer += mDeltaTime;

			// スライディング中は専用の摩擦を適用
			ApplyFriction(mSlideFriction);

			// スライディング中は下方向への力を継続的に適用
			mVelocity.y -= Math::HtoM(mSlideDownForce) * mDeltaTime;

			// カメラ方向と現在のスライド方向を加味して方向を調整（段階的に）
			if (!mMoveInput.IsZero()) {
				Vec3 cameraForward = CameraManager::GetActiveCamera()->
				                     GetViewMat().Inverse().GetForward();
				cameraForward.y = 0.0f;
				cameraForward.Normalize();
				Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

				// 入力に基づいて方向を計算
				Vec3 inputDir = (cameraForward * mMoveInput.z) + (cameraRight *
					mMoveInput.x);
				inputDir.y = 0.0f;
				inputDir.Normalize();

				// スライド方向を徐々に入力方向へ補間（スライド中は制限付き）
				float turnSpeed = 0.2f; // スライド中の方向転換の強さ
				mSlideDir       = Math::Lerp(mSlideDir, inputDir,
				                             turnSpeed * mDeltaTime);
				mSlideDir.Normalize();

				// 方向調整の度に若干減速させる
				float currentSpeed = mVelocity.Length();
				mVelocity          = mSlideDir * (currentSpeed * 0.98f);
			}

			// スライディング終了条件（改良版）- 最小スライド時間を導入
			if (mSlideTimer >= kMinSlideTime) {
				// 最小スライド時間以上経過している場合のみ終了条件を考慮
				bool shouldEndSlide = false;

				if (mSlideTimer >= mAxSlideTime) {
					// 最大スライド時間を超えた場合は終了
					shouldEndSlide = true;
				}

				// 接地していない場合は終了
				if (!mWasGrounded) {
					shouldEndSlide = true;
				}
				// しゃがみボタンを押し続けている場合は速度条件のみで判断
				else if (mWishCrouch) {
					// 速度が著しく低下した場合のみ終了
					if (Math::MtoH(mVelocity).Length() < mSlideMinSpeed *
						0.3f) {
						shouldEndSlide = true;
					}
				}
				// しゃがみボタンを離した場合は終了
				else {
					shouldEndSlide = true;
				}

				if (shouldEndSlide) {
					mSlideState         = SlideRecovery;
					mSlideRecoveryTimer = 0.0f;
				}
			}
		}
		// スライド回復中の処理
		else if (mSlideState == SlideRecovery) {
			mSlideRecoveryTimer += mDeltaTime;

			// スライド回復中は新たなスライドを開始できないようにする
			// 回復時間が経過したら立ち上がる
			if (mSlideRecoveryTimer >= mSlideRecoveryTime) {
				mSlideState = NotSliding;
				if (!mWishCrouch) {
					mIsCrouching     = false;
					mCurrentHeightHu = kDefaultHeightHU;
				}
			}

			ApplyFriction(
				ConVarManager::GetConVar("sv_friction")->GetValueAsFloat());
		} else {
			// 通常時の摩擦を適用
			ApplyFriction(
				ConVarManager::GetConVar("sv_friction")->GetValueAsFloat());
		}

	// ジャンプ処理 - スライド状態をリセット
	if (mIsGrounded && InputSystem::IsPressed("+jump")) {
		mVelocity.y = Math::HtoM(mJumpVel);
		mIsGrounded = false; // ジャンプ中は接地判定を解除

		// ジャンプ時にスライド状態をリセット
		if (mSlideState != NotSliding) {
			mSlideState = NotSliding;
		}

		mCanDoubleJump      = true;
		mDoubleJumped       = false;
		mJumpKeyWasReleased = false;

		// ジャンプ時のカメラシェイクを開始
		StartCameraShake(1.0f, 0.01f, 0.005f, Vec3::up, 0.025f, 0.5f,
		                 Vec3::right);
	} else if (!mIsGrounded) {
		if (!InputSystem::IsPressed("+jump")) {
			// ジャンプキーが離された場合、二段ジャンプのフラグをリセット
			mJumpKeyWasReleased = true;
		} else if (mCanDoubleJump && !mDoubleJumped && mJumpKeyWasReleased) {
			mVelocity.y   = Math::HtoM(mJumpVel);
			mDoubleJumped = true;

			StartCameraShake(1.0f, 0.01f, 0.005f, Vec3::up, 0.025f, 0.5f,
			                 Vec3::right);
		}
	}

	// CheckVelocity
	CheckVelocity();

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera        = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();

	cameraForward.y = 0.0f; // Y成分を0にする
	cameraForward.Normalize();

	// カメラの右方向ベクトルを計算（Y軸との外積）
	Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

	// 入力に基づいて移動方向を計算
	Vec3 wishvel = (cameraForward * mMoveInput.z) + (cameraRight * mMoveInput.
		x);
	wishvel.y = 0.0f;

	if (!wishvel.IsZero()) {
		wishvel.Normalize();
	}

	if (mWasGrounded) {
		mWishdir        = wishvel;
		float wishspeed = mWishdir.Length() * mSpeed;
		float maxspeed  = ConVarManager::GetConVar("sv_maxspeed")->
			GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}

		Accelerate(mWishdir, wishspeed,
		           ConVarManager::GetConVar(
			           "sv_accelerate")->GetValueAsFloat());

		//// 地面にいたらベロシティを地面の法線方向に投影
		//velocity_ = velocity_ - (normal_ * velocity_.Dot(normal_));
	} else {
		mWishdir        = wishvel;
		float wishspeed = mWishdir.Length() * mSpeed;
		float maxspeed  = ConVarManager::GetConVar("sv_maxspeed")->
			GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		AirAccelerate(mWishdir, wishspeed,
		              ConVarManager::GetConVar("sv_airaccelerate")->
		              GetValueAsFloat());
	}

	if (!mIsGrounded) {
		ApplyHalfGravity();
	}

	CheckVelocity();
}

void PlayerMovementUPhysics::SetIsGrounded(const bool cond) {
	mIsGrounded = cond;
}

void PlayerMovementUPhysics::SetVelocity(const Vec3 newVel) {
	mVelocity = newVel;
}

Vec3 PlayerMovementUPhysics::GetHeadPos() const {
	return CharacterMovement::GetHeadPos();
}

void PlayerMovementUPhysics::StartCameraShake(
	float       duration, float        amplitude,
	float       frequency, const Vec3& direction,
	float       rotationAmplitude,
	float       rotationFrequency,
	const Vec3& rotationAxis) {
	// 既存のシェイクがアクティブな場合は蓄積
	if (mCameraShake.isActive) {
		// 残り時間の長い方を採用
		mCameraShake.duration = (std::max)(
			mCameraShake.duration - mCameraShake.currentTime, duration);

		// 振幅を加算（最大値を超えないよう制限）
		mCameraShake.baseAmplitude += amplitude;
		mCameraShake.amplitude = (std::min)(mCameraShake.baseAmplitude,
		                                    mCameraShake.maxAmplitude);

		// 回転振幅を加算（最大値を超えないよう制限）
		mCameraShake.baseRotationAmplitude += rotationAmplitude;
		mCameraShake.rotationAmplitude = (std::min)(
			mCameraShake.baseRotationAmplitude,
			mCameraShake.maxRotationAmplitude);

		// 周波数は高い方を優先
		mCameraShake.frequency = (std::max)(mCameraShake.frequency, frequency);
		mCameraShake.rotationFrequency = (std::max)(
			mCameraShake.rotationFrequency, rotationFrequency);

		// 方向と回転軸は平均化して正規化
		mCameraShake.direction = (mCameraShake.direction + direction).
			Normalized();
		mCameraShake.rotationAxis = (mCameraShake.rotationAxis + rotationAxis).
			Normalized();
	} else {
		// 新規シェイクの場合は普通に初期化
		mCameraShake.duration      = duration;
		mCameraShake.currentTime   = 0.0f;
		mCameraShake.amplitude     = amplitude;
		mCameraShake.baseAmplitude = amplitude;
		mCameraShake.frequency     = frequency;
		mCameraShake.direction     = direction.Normalized();

		// 回転揺れパラメータの設定
		mCameraShake.rotationAmplitude     = rotationAmplitude;
		mCameraShake.baseRotationAmplitude = rotationAmplitude;
		mCameraShake.rotationFrequency     = rotationFrequency;
		mCameraShake.rotationAxis          = rotationAxis.Normalized();

		// 最大値の設定（調整可能）
		mCameraShake.maxAmplitude         = 0.1f;  // 適切な最大振幅
		mCameraShake.maxRotationAmplitude = 0.05f; // 適切な最大回転振幅
	}

	mCameraShake.isActive = true;
}

void PlayerMovementUPhysics::UpdateCameraShake(const float deltaTime) {
	if (!mCameraShake.isActive) {
		return;
	}

	mCameraShake.currentTime += deltaTime;

	// 揺れの終了判定
	if (mCameraShake.currentTime >= mCameraShake.duration) {
		mCameraShake.isActive = false;
		// シェイク終了時にベース値をリセット
		mCameraShake.baseAmplitude         = 0.0f;
		mCameraShake.baseRotationAmplitude = 0.0f;
		return;
	}

	// 時間経過に応じた減衰係数（終了に近づくほど揺れが小さくなる）
	float dampingFactor = 1.0f - (mCameraShake.currentTime / mCameraShake.
		duration);

	// イージング関数で開始・終了をスムーズに
	float easedDamping = dampingFactor * dampingFactor * (3.0f - 2.0f *
		dampingFactor);

	// 現在の振幅に減衰を適用
	//float currentAmplitude = cameraShake_.amplitude * easedDamping;

	// 現在の回転振幅に減衰を適用
	float rotAmount = mCameraShake.rotationAmplitude * easedDamping;

	float rotNoise1 = std::sin(
		mCameraShake.currentTime * mCameraShake.rotationFrequency * 12.0f +
		0.5f);
	float rotNoise2 = std::cos(
		mCameraShake.currentTime * mCameraShake.rotationFrequency * 9.0f +
		1.2f);
	float rotNoise3 = std::sin(
		mCameraShake.currentTime * mCameraShake.rotationFrequency * 6.0f +
		2.7f);

	// 主軸と直交する軸を計算
	Vec3 perpAxis1, perpAxis2;
	if (std::abs(mCameraShake.rotationAxis.Dot(Vec3::up)) < 0.9f) {
		perpAxis1 = Vec3::up.Cross(mCameraShake.rotationAxis).Normalized();
	} else {
		perpAxis1 = Vec3::forward.Cross(mCameraShake.rotationAxis).Normalized();
	}
	perpAxis2 = mCameraShake.rotationAxis.Cross(perpAxis1).Normalized();

	// 各エンティティに揺れを適用
	for (const auto& entityInfo : mShakeEntities) {
		if (!entityInfo.entity) continue;

		// カメラのトランスフォーム取得
		SceneComponent* entTransform = entityInfo.entity->GetTransform();
		if (entTransform) {
			// 現在の回転を取得
			Quaternion currentRot = Quaternion::identity; // ローカルなので単位

			// エンティティごとの揺れ強度を適用
			float entityRotAmount = rotAmount * entityInfo.rotationMultiplier;

			// 3つの軸それぞれに回転揺れを適用
			Quaternion rotShake = Quaternion::identity;
			rotShake            = rotShake * Quaternion::AxisAngle(
				mCameraShake.rotationAxis, entityRotAmount * rotNoise1);
			rotShake = rotShake * Quaternion::AxisAngle(
				perpAxis1, entityRotAmount * 0.7f * rotNoise2);
			rotShake = rotShake * Quaternion::AxisAngle(
				perpAxis2, entityRotAmount * 0.4f * rotNoise3);

			// カメラ回転を更新
			entTransform->SetLocalRot(currentRot * rotShake);
		}
	}
}

// -----------------------------------------------------------------------------
// PlayerMovementUPhysics::CollideAndSlide
// -----------------------------------------------------------------------------
// 目的 : 「スタックしない & 複数面スライド」の衝突応答
// 引数 : desiredDisplacement … このフレーム中に進みたいワールド座標差分
// 前提 : mDeltaTime で割ると “本来の速度 (m/s)” になる
// -----------------------------------------------------------------------------
void PlayerMovementUPhysics::CollideAndSlide(const Vec3& desiredDisplacement) {
	BoxColliderComponent* col = mOwner->GetComponent<BoxColliderComponent>();
	if (!col || !mUPhysicsEngine) {
		mPosition += desiredDisplacement;
		return;
	}

	// ---------------------------------------------------------------------
	// 定数定義
	// ---------------------------------------------------------------------
	constexpr int   kMaxPlanes     = 5;      // 同時に考慮する面の最大数
	constexpr int   kMaxIters      = 4;      // 1フレームで許す再試行回数
	constexpr float kPushOutHU     = 1.0f;   // めり込み押し出し量 (HammerUnit)
	constexpr float kEpsHU         = 0.2f;   // 浮動小数の誤差吸収用 ε
	constexpr float kStopSpeed     = 0.1f;   // これ未満なら停止とみなす (m/s)
	constexpr float kPlaneMergeCos = 0.997f; // ≒5°5
	constexpr float kWalkableCos   = 0.7f;   // 床判定 (≈45°)
	constexpr float kNormalBlendHz = 20.0f;
	const float     kStepMaxM      = Math::HtoM(18.0f); // 18HU
	constexpr float kEdgeCos       = 0.02f;


	// ---------------------------------------------------------------------
	// 初期データ
	// ---------------------------------------------------------------------
	const Vec3 boxHalf = col->GetBoundingBox().GetHalfSize();
	const Vec3 offset  = col->GetOffset();

	Vec3 curPos = mScene->GetWorldPos() + offset;
	Vec3 curVel = desiredDisplacement / mDeltaTime; // m/s
	Vec3 planes[kMaxPlanes];
	int  numPlanes = 0;

	//-------------------------------------------------------------------------
	// 面の法線を追加するためのラムダ関数
	//-------------------------------------------------------------------------
	auto PushPlane = [&](const Vec3& n) {
		for (int i = 0; i < numPlanes; ++i) {
			if (planes[i].Dot(n) > kPlaneMergeCos) {
				return; // ほぼ同じ → 捨てる
			}
		}
		if (numPlanes < kMaxPlanes) planes[numPlanes++] = n;
	};

	// ---------------------------------------------------------------------
	// 0. すでにめり込んでいる場合は押し出す
	// ---------------------------------------------------------------------
	{
		UPhysics::Box startBox{.center = curPos, .half = boxHalf};
		UPhysics::Hit overlapHit{};
		if (mUPhysicsEngine->BoxOverlap(startBox, &overlapHit)) {
			// Source では ResolvePenetration と呼ばれる処理
			curPos += overlapHit.normal * (overlapHit.depth + Math::HtoM(
				kPushOutHU));
		}
	}

	// ---------------------------------------------------------------------
	// 1. Sweep → Hit → 速度クリップ → 残り距離で再試行 … を最大 kMaxIters 回
	// ---------------------------------------------------------------------
	float remainingTime = mDeltaTime;

	for (int iter = 0; iter < kMaxIters && remainingTime > 0.0f; ++iter) {
		// 今回移動したい距離と方向
		Vec3  moveDelta = curVel * remainingTime;
		float moveDist  = moveDelta.Length();

		if (moveDist < kStopSpeed * remainingTime) {
			// ほぼ停止
			break;
		}

		Vec3 dir = moveDelta / moveDist; // 正規化方向

		// Sweep
		UPhysics::Box castBox{.center = curPos, .half = boxHalf};
		UPhysics::Hit hit{};
		bool          hasHit = mUPhysicsEngine->BoxCast(
			castBox, dir, moveDist + Math::HtoM(kEpsHU),
			&hit
		);
		if (!hasHit) {
			// 衝突なし。残り距離すべて進んで終了
			curPos += moveDelta;
			break;
		}

		// 1-a. 衝突手前まで進む
		float travel = std::max(hit.t * moveDist - Math::HtoM(kEpsHU), 0.0f);
		curPos += dir * travel;

		// -------------------------------------------------------------
		// 1-b. 段差チェック
		// -------------------------------------------------------------
		bool canStep =
			hit.normal.y > kWalkableCos &&
			hit.pos.y <= curPos.y + kStepMaxM;
		if (canStep) {
			Vec3 horizontal = curVel - Vec3::up * curVel.y;
			curPos += horizontal * remainingTime; // 水平分だけ上げる
			mGroundNormal = hit.normal;
			mIsGrounded   = true;
			break; // 早期終了
		}

		// -------------------------------------------------------------
		// 1-c. エッジ平均法線を生成
		// -------------------------------------------------------------
		PushPlane(hit.normal);
		numPlanes = std::max(numPlanes, 1); // 1 枚も無い事態を防ぐ

		UPhysics::Hit nearHits[4];
		int           nh = mUPhysicsEngine->BoxOverlap(
			{.center = curPos, .half = boxHalf},
			nearHits, 4); // ← 戻り値でヒット数取得

		Vec3 avgN = Vec3::zero;
		for (int i = 0; i < nh; ++i) {
			avgN += nearHits[i].normal;
		}
		
		if (nh > 1 && (avgN / static_cast<float>(nh)).Dot(hit.normal) <
			kEdgeCos) {
			PushPlane((avgN / static_cast<float>(nh)).Normalized());
		}

		// 1-d. ClipVelocity
		//      v' = v - n * (v·n)  ― すべての面
		for (int i = 0; i < numPlanes; ++i) {
			float into = curVel.Dot(planes[i]);
			if (into < 0.0f) {
				curVel -= planes[i] * into;
			}
		}

		// 交差する「歩けない」面が 2 枚以上ある場合、エッジ方向へ投影
		if (numPlanes >= 2) {
			// 交差エッジ方向は面法線のクロス積
			Vec3 dirEdge = planes[0].Cross(planes[1]);
			if (!dirEdge.IsZero()) {
				dirEdge.Normalize();
				// エッジに投影
				curVel = dirEdge * curVel.Dot(dirEdge);
			}
		}

		// 速度が潰れたら停止
		if (curVel.SqrLength() < kStopSpeed * kStopSpeed) {
			curVel = Vec3::zero;
			break;
		}

		// このフレームで残った時間を再計算
		remainingTime = (1.0f - hit.t) * remainingTime;
	}

	// ---------------------------------------------------------------------
	// 2. 位置と速度を確定
	// ---------------------------------------------------------------------
	mPosition = curPos - offset;

	// 新たな Ground 判定
	bool thisFrameGrounded = false;
	for (int i = 0; i < numPlanes; ++i) {
		if (planes[i].y >= kWalkableCos) {
			thisFrameGrounded = true;
			// Exp-Lerp で 20Hz 収束 (Δt 毎)
			float a       = 1.0f - std::exp(-kNormalBlendHz * mDeltaTime);
			mGroundNormal = Math::Lerp(mGroundNormal, planes[i], a).
				Normalized();
			break;
		}
	}

	// 速度を “平均床面” にプロジェクション
	if (thisFrameGrounded) {
		curVel -= mGroundNormal * curVel.Dot(mGroundNormal);
	}

	mVelocity = curVel;

	// grounded フラグを次へ
	mIsGrounded = thisFrameGrounded;
}
