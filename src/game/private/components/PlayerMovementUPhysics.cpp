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

	mIsGrounded = CheckGrounded();

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

	// if (isGrounded_ && velocity_.y < 0.0f) {
	//	velocity_.y = 0.0f; // 落下中のみリセット
	// }

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

void PlayerMovementUPhysics::CollideAndSlide(const Vec3& disp) {
	auto* col = mOwner->GetComponent<BoxColliderComponent>();
	if (!col || !mUPhysicsEngine) {
		mPosition += disp;
		return;
	}

	constexpr int kMaxPlanes = 5; // ← Source Engine と同数
	constexpr int kMaxIters  = 8;

	constexpr float kEpsHU     = 0.25f; // 0.2 HU ≈ 5 mm
	constexpr float kPushOutHU = 0.25f; // 1.0 HU ≈ 2.5 cm

	const float kEps     = Math::HtoM(kEpsHU);
	const float kPushOut = Math::HtoM(kPushOutHU);

	Vec3 planes[kMaxPlanes];
	int  nPlanes = 0;

	Vec3 pos     = mScene->GetWorldPos() + col->GetOffset();
	Vec3 vel     = disp; // 今フレームで消費したい “速度”
	Vec3 boxHalf = col->GetBoundingBox().GetHalfSize();

	for (int iter = 0; iter < kMaxIters && vel.SqrLength() > kEps * kEps; ++
	     iter) {
		// ── Sweep (BoxCast) ─────────────────────────
		UPhysics::Box box{pos, boxHalf};
		UPhysics::Hit hit{};
		float         moveLen = vel.Length();
		Vec3          moveDir = vel / moveLen;

		if (!mUPhysicsEngine->BoxCast(box, moveDir, moveLen, &hit)) {
			pos += vel;
			break;
		}

		// ── 1) ヒット点まで進む（ε 手前） ─────────────
		float travel = std::max(hit.t * moveLen - kEps, 0.f);
		pos += moveDir * travel;

		// ── 2) めり込み保険 ─────────────────────────
		if (hit.t * moveLen < kEps) pos += hit.normal * kPushOut;

		// ── 3) 法線スタックへ追加（重複チェック付き） ──
		bool already = false;
		for (int i = 0; i < nPlanes; ++i)
			if (hit.normal.Dot(planes[i]) > 0.99f) {
				already = true;
				break;
			}
		if (!already && nPlanes < kMaxPlanes) planes[nPlanes++] = hit.normal;
	}

	// ── 4) 速度を全法線で“まとめて”クリップ ────────
	for (int i = 0; i < nPlanes; ++i) {
		float d = vel.Dot(planes[i]);
		if (d < 0.f) vel -= planes[i] * d; // 法線成分を除去
	}

	// ── 5) 位置・速度を反映 ─────────────────────────
	mPosition = pos - col->GetOffset();
	if (mDeltaTime > 0.f) {
		mVelocity = vel / mDeltaTime;
	} else {
		mVelocity = Vec3::zero;
	}
}
