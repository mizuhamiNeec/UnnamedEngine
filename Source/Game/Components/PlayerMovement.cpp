#include "PlayerMovement.h"

#include <algorithm>

#include <Camera/CameraManager.h>

#include <Components/Camera/CameraComponent.h>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <Input/InputSystem.h>

#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/Vector/Vec3.h>

#include "Components/ColliderComponent/BoxColliderComponent.h"
#include "Components/ColliderComponent/Base/ColliderComponent.h"

#include "ImGuiManager/ImGuiManager.h"
#include "ImGuiManager/ImGuiWidgets.h"

#include "Lib/DebugHud/DebugHud.h"

#include "Physics/PhysicsEngine.h"

PlayerMovement::~PlayerMovement() {
}

void PlayerMovement::OnAttach(Entity& owner) {
	CharacterMovement::OnAttach(owner);

	// プレイヤー固有の初期化処理
	mSpeed   = 290.0f;
	mJumpVel = 350.0f;

	// Console::SubmitCommand("sv_gravity 0");
}

void PlayerMovement::ProcessInput() {
	//-------------------------------------------------------------------------
	// 移動入力
	//-------------------------------------------------------------------------
	moveInput_ = {0.0f, 0.0f, 0.0f};

	if (InputSystem::IsPressed("forward")) {
		moveInput_.z += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		moveInput_.z -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		moveInput_.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		moveInput_.x -= 1.0f;
	}

	if (InputSystem::IsPressed("crouch")) {
		bWishCrouch = true;
	}

	if (InputSystem::IsReleased("crouch")) {
		bWishCrouch = false;
	}

	moveInput_.Normalize();
}

void PlayerMovement::Update([[maybe_unused]] const float deltaTime) {
	// プレイヤー固有の処理
	ProcessInput();

	// 前回状態を保存
	bWasGrounded        = bIsGrounded;
	previousSlideState_ = slideState;

	if (bWishCrouch && !bIsCrouching) {
		bIsCrouching     = true;
		mCurrentHeightHU = kDefaultHeightHU * 0.5f;
	} else if (!bWishCrouch && bIsCrouching) {
		bIsCrouching     = false;
		mCurrentHeightHU = kDefaultHeightHU;
	}

	// 衝突検出とスライド処理（プレイヤー固有）
	CollideAndSlide(velocity_ * deltaTime);

	// 基底クラスの共通更新処理を呼び出し
	CharacterMovement::Update(deltaTime);

	// スライディング開始の検出とイベント実行
	if (slideState == Sliding && previousSlideState_ != Sliding) {
		// スライディング開始時のカメラシェイク（初回のみ）
		StartCameraShake(0.4f, 0.03f, 5.0f, Vec3::forward, 0.015f, 3.5f,
		                 Vec3::right);
	}

	UpdateCameraShake(deltaTime);

	// プレイヤー固有のデバッグ描画
	Debug::DrawRay(transform_->GetWorldPos(), wishdir_, Vec4::cyan);

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera        = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();

	auto* collider = owner_->GetComponent<ColliderComponent>();

	if (!collider) {
		Console::Print(
			"PlayerMovement::CollideAndSlide() : ColliderComponent is not attached to the owner entity.",
			Vec4::yellow,
			Channel::Physics
		);
	} else {
		const std::vector<HitResult> hits = collider->RayCast(
			camera->GetViewMat().Inverse().GetTranslate(), cameraForward,
			100.0f);

		Debug::DrawRay(
			camera->GetViewMat().Inverse().GetTranslate() + camera->GetViewMat()
			.Inverse().GetForward() * 2.0f, cameraForward * 98.0f, Vec4::red);
		for (auto hit : hits) {
			if (hit.isHit) {
				Debug::DrawBox(hit.hitPos, Quaternion::identity,
				               Vec3(0.1f, 0.1f, 0.1f), Vec4::green);
				Debug::DrawAxis(hit.hitPos, Quaternion::identity);
				Debug::DrawRay(hit.hitPos, hit.hitNormal, Vec4::magenta);
			}
		}
	}
}

static float kEdgeAngleThreshold = 0.2f; // エッジ判定の閾値

void PlayerMovement::DrawInspectorImGui() {
	#ifdef _DEBUG
	if (ImGui::CollapsingHeader("PlayerMovement",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGuiWidgets::DragVec3(
			"Velocity",
			velocity_,
			Vec3::zero,
			0.1f,
			"%.2f m/s"
		);
		ImGui::DragFloat("edge angle threshold", &kEdgeAngleThreshold, 0.01f,
		                 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("State Info:");
		ImGui::Text("IsGrounded: %s", bIsGrounded ? "Yes" : "No");
		ImGui::Text("WasGrounded: %s", bWasGrounded ? "Yes" : "No");

		const char* slideStateStr =
			slideState == NotSliding
				? "Not Sliding"
				: (slideState == Sliding ? "Sliding" : "Recovery");

		const char* prevSlideStateStr =
			previousSlideState_ == NotSliding
				? "Not Sliding"
				: (previousSlideState_ == Sliding ? "Sliding" : "Recovery");

		ImGui::Text("Slide State: %s (Prev: %s)", slideStateStr,
		            prevSlideStateStr);
		ImGui::Text("State Changed: %s",
		            (slideState != previousSlideState_) ? "Yes" : "No");

		// 現在適用されている摩擦値の表示
		float currentFriction = slideState == Sliding
			                        ? slideFriction
			                        : ConVarManager::GetConVar("sv_friction")->
			                        GetValueAsFloat();
		ImGui::Text("Current Friction: %.4f", currentFriction);
	}
	#endif
}

Vec3 ProjectOnPlane(const Vec3& vector, const Vec3& normal) {
	return vector - normal * vector.Dot(normal);
}

Vec3 GetMoveDirection(const Vec3& forward, const Vec3& groundNormal) {
	Vec3 projectedForward = ProjectOnPlane(forward, groundNormal);
	return projectedForward.Normalized();
}

void PlayerMovement::Move() {
	// 前回のフレームの接地状態を保存
	bWasGrounded        = bIsGrounded;
	previousSlideState_ = slideState;

	if (!bIsGrounded) {
		ApplyHalfGravity();
	}

	bIsGrounded = CheckGrounded();

	if (!bWasGrounded && bIsGrounded) {
		// 着地処理

		// スライド状態でない場合のみ通常摩擦に設定
		if (slideState != Sliding) {
			// 着地時の衝撃が大きい場合のみカメラシェイク
			if (velocity_.y < -Math::HtoM(200.0f)) {
				float landingForce = -velocity_.y / Math::HtoM(350.0f);
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
	if (bIsGrounded && bWishCrouch && slideState == NotSliding) {
		float currentSpeed = Math::MtoH(velocity_).Length();
		if (currentSpeed > slideMinSpeed) {
			// スライディング開始
			slideState = Sliding;
			slideTimer = 0.0f;

			// しゃがみ状態設定
			bIsCrouching     = true;
			mCurrentHeightHU = kDefaultHeightHU * 0.5f;

			// スライド方向を現在の速度方向に設定
			mSlideDir = velocity_.Normalized();

			// 前方向に加速
			velocity_ += mSlideDir * slideBoost;

			// 下方向への力を加えて斜面を下りやすくする
			velocity_.y -= Math::HtoM(slideDownForce) * deltaTime_;
		}
	} else
	// スライディング中の処理
		if (slideState == Sliding) {
			slideTimer += deltaTime_;

			// スライディング中は専用の摩擦を適用
			ApplyFriction(slideFriction);

			// スライディング中は下方向への力を継続的に適用
			velocity_.y -= Math::HtoM(slideDownForce) * deltaTime_;

			// カメラ方向と現在のスライド方向を加味して方向を調整（段階的に）
			if (!moveInput_.IsZero()) {
				Vec3 cameraForward = CameraManager::GetActiveCamera()->
				                     GetViewMat().Inverse().GetForward();
				cameraForward.y = 0.0f;
				cameraForward.Normalize();
				Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

				// 入力に基づいて方向を計算
				Vec3 inputDir = (cameraForward * moveInput_.z) + (cameraRight *
					moveInput_.x);
				inputDir.y = 0.0f;
				inputDir.Normalize();

				// スライド方向を徐々に入力方向へ補間（スライド中は制限付き）
				float turnSpeed = 0.2f; // スライド中の方向転換の強さ
				mSlideDir       = Math::Lerp(mSlideDir, inputDir,
				                             turnSpeed * deltaTime_);
				mSlideDir.Normalize();

				// 方向調整の度に若干減速させる
				float currentSpeed = velocity_.Length();
				velocity_          = mSlideDir * (currentSpeed * 0.98f);
			}

			// スライディング終了条件（改良版）- 最小スライド時間を導入
			if (slideTimer >= kMinSlideTime) {
				// 最小スライド時間以上経過している場合のみ終了条件を考慮
				bool shouldEndSlide = false;

				if (slideTimer >= maxSlideTime) {
					// 最大スライド時間を超えた場合は終了
					shouldEndSlide = true;
				}

				// 接地していない場合は終了
				if (!bWasGrounded) {
					shouldEndSlide = true;
				}
				// しゃがみボタンを押し続けている場合は速度条件のみで判断
				else if (bWishCrouch) {
					// 速度が著しく低下した場合のみ終了
					if (Math::MtoH(velocity_).Length() < slideMinSpeed * 0.3f) {
						shouldEndSlide = true;
					}
				}
				// しゃがみボタンを離した場合は終了
				else {
					shouldEndSlide = true;
				}

				if (shouldEndSlide) {
					slideState         = SlideRecovery;
					slideRecoveryTimer = 0.0f;
				}
			}
		}
		// スライド回復中の処理
		else if (slideState == SlideRecovery) {
			slideRecoveryTimer += deltaTime_;

			// スライド回復中は新たなスライドを開始できないようにする
			// 回復時間が経過したら立ち上がる
			if (slideRecoveryTimer >= slideRecoveryTime) {
				slideState = NotSliding;
				if (!bWishCrouch) {
					bIsCrouching     = false;
					mCurrentHeightHU = kDefaultHeightHU;
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
	if (bIsGrounded && InputSystem::IsPressed("+jump")) {
		velocity_.y = Math::HtoM(mJumpVel);
		bIsGrounded = false; // ジャンプ中は接地判定を解除

		// ジャンプ時にスライド状態をリセット
		if (slideState != NotSliding) {
			slideState = NotSliding;
		}

		bCanDoubleJump_      = true;
		bDoubleJumped_       = false;
		bJumpKeyWasReleased_ = false;

		// ジャンプ時のカメラシェイクを開始
		StartCameraShake(1.0f, 0.01f, 0.005f, Vec3::up, 0.025f, 0.5f,
		                 Vec3::right);
	} else if (!bIsGrounded) {
		if (!InputSystem::IsPressed("+jump")) {
			// ジャンプキーが離された場合、二段ジャンプのフラグをリセット
			bJumpKeyWasReleased_ = true;
		} else if (bCanDoubleJump_ && !bDoubleJumped_ && bJumpKeyWasReleased_) {
			velocity_.y    = Math::HtoM(mJumpVel);
			bDoubleJumped_ = true;

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
	Vec3 wishvel = (cameraForward * moveInput_.z) + (cameraRight * moveInput_.
		x);
	wishvel.y = 0.0f;

	if (!wishvel.IsZero()) {
		wishvel.Normalize();
	}

	if (bWasGrounded) {
		wishdir_        = wishvel;
		float wishspeed = wishdir_.Length() * mSpeed;
		float maxspeed  = ConVarManager::GetConVar("sv_maxspeed")->
			GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}

		Accelerate(wishdir_, wishspeed,
		           ConVarManager::GetConVar(
			           "sv_accelerate")->GetValueAsFloat());

		//// 地面にいたらベロシティを地面の法線方向に投影
		//velocity_ = velocity_ - (normal_ * velocity_.Dot(normal_));
	} else {
		wishdir_        = wishvel;
		float wishspeed = wishdir_.Length() * mSpeed;
		float maxspeed  = ConVarManager::GetConVar("sv_maxspeed")->
			GetValueAsFloat();
		if (wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		AirAccelerate(wishdir_, wishspeed,
		              ConVarManager::GetConVar("sv_airaccelerate")->
		              GetValueAsFloat());
	}

	if (!bIsGrounded) {
		ApplyHalfGravity();
	}

	CheckVelocity();
}

void PlayerMovement::SetIsGrounded(const bool cond) {
	bIsGrounded = cond;
}

void PlayerMovement::SetVelocity(const Vec3 newVel) {
	velocity_ = newVel;
}

Vec3 PlayerMovement::GetHeadPos() const {
	return CharacterMovement::GetHeadPos();
}

void PlayerMovement::StartCameraShake(
	float       duration, float        amplitude,
	float       frequency, const Vec3& direction,
	float       rotationAmplitude,
	float       rotationFrequency,
	const Vec3& rotationAxis) {
	// 既存のシェイクがアクティブな場合は蓄積
	if (cameraShake_.isActive) {
		// 残り時間の長い方を採用
		cameraShake_.duration = (std::max)(
			cameraShake_.duration - cameraShake_.currentTime, duration);

		// 振幅を加算（最大値を超えないよう制限）
		cameraShake_.baseAmplitude += amplitude;
		cameraShake_.amplitude = (std::min)(cameraShake_.baseAmplitude,
		                                    cameraShake_.maxAmplitude);

		// 回転振幅を加算（最大値を超えないよう制限）
		cameraShake_.baseRotationAmplitude += rotationAmplitude;
		cameraShake_.rotationAmplitude = (std::min)(
			cameraShake_.baseRotationAmplitude,
			cameraShake_.maxRotationAmplitude);

		// 周波数は高い方を優先
		cameraShake_.frequency = (std::max)(cameraShake_.frequency, frequency);
		cameraShake_.rotationFrequency = (std::max)(
			cameraShake_.rotationFrequency, rotationFrequency);

		// 方向と回転軸は平均化して正規化
		cameraShake_.direction = (cameraShake_.direction + direction).
			Normalized();
		cameraShake_.rotationAxis = (cameraShake_.rotationAxis + rotationAxis).
			Normalized();
	} else {
		// 新規シェイクの場合は普通に初期化
		cameraShake_.duration      = duration;
		cameraShake_.currentTime   = 0.0f;
		cameraShake_.amplitude     = amplitude;
		cameraShake_.baseAmplitude = amplitude;
		cameraShake_.frequency     = frequency;
		cameraShake_.direction     = direction.Normalized();

		// 回転揺れパラメータの設定
		cameraShake_.rotationAmplitude     = rotationAmplitude;
		cameraShake_.baseRotationAmplitude = rotationAmplitude;
		cameraShake_.rotationFrequency     = rotationFrequency;
		cameraShake_.rotationAxis          = rotationAxis.Normalized();

		// 最大値の設定（調整可能）
		cameraShake_.maxAmplitude         = 0.1f;  // 適切な最大振幅
		cameraShake_.maxRotationAmplitude = 0.05f; // 適切な最大回転振幅
	}

	cameraShake_.isActive = true;
}

void PlayerMovement::UpdateCameraShake(const float deltaTime) {
	if (!cameraShake_.isActive) {
		return;
	}

	cameraShake_.currentTime += deltaTime;

	// 揺れの終了判定
	if (cameraShake_.currentTime >= cameraShake_.duration) {
		cameraShake_.isActive = false;
		// シェイク終了時にベース値をリセット
		cameraShake_.baseAmplitude         = 0.0f;
		cameraShake_.baseRotationAmplitude = 0.0f;
		return;
	}

	// 時間経過に応じた減衰係数（終了に近づくほど揺れが小さくなる）
	float dampingFactor = 1.0f - (cameraShake_.currentTime / cameraShake_.
		duration);

	// イージング関数で開始・終了をスムーズに
	float easedDamping = dampingFactor * dampingFactor * (3.0f - 2.0f *
		dampingFactor);

	// 現在の振幅に減衰を適用
	//float currentAmplitude = cameraShake_.amplitude * easedDamping;

	// 現在の回転振幅に減衰を適用
	float rotAmount = cameraShake_.rotationAmplitude * easedDamping;

	float rotNoise1 = std::sin(
		cameraShake_.currentTime * cameraShake_.rotationFrequency * 12.0f +
		0.5f);
	float rotNoise2 = std::cos(
		cameraShake_.currentTime * cameraShake_.rotationFrequency * 9.0f +
		1.2f);
	float rotNoise3 = std::sin(
		cameraShake_.currentTime * cameraShake_.rotationFrequency * 6.0f +
		2.7f);

	// 主軸と直交する軸を計算
	Vec3 perpAxis1, perpAxis2;
	if (std::abs(cameraShake_.rotationAxis.Dot(Vec3::up)) < 0.9f) {
		perpAxis1 = Vec3::up.Cross(cameraShake_.rotationAxis).Normalized();
	} else {
		perpAxis1 = Vec3::forward.Cross(cameraShake_.rotationAxis).Normalized();
	}
	perpAxis2 = cameraShake_.rotationAxis.Cross(perpAxis1).Normalized();

	// 各エンティティに揺れを適用
	for (const auto& entityInfo : shakeEntities_) {
		if (!entityInfo.entity) continue;

		// カメラのトランスフォーム取得
		TransformComponent* entTransform = entityInfo.entity->GetTransform();
		if (entTransform) {
			// 現在の回転を取得
			Quaternion currentRot = Quaternion::identity; // ローカルなので単位

			// エンティティごとの揺れ強度を適用
			float entityRotAmount = rotAmount * entityInfo.rotationMultiplier;

			// 3つの軸それぞれに回転揺れを適用
			Quaternion rotShake = Quaternion::identity;
			rotShake            = rotShake * Quaternion::AxisAngle(
				cameraShake_.rotationAxis, entityRotAmount * rotNoise1);
			rotShake = rotShake * Quaternion::AxisAngle(
				perpAxis1, entityRotAmount * 0.7f * rotNoise2);
			rotShake = rotShake * Quaternion::AxisAngle(
				perpAxis2, entityRotAmount * 0.4f * rotNoise3);

			// カメラ回転を更新
			entTransform->SetLocalRot(currentRot * rotShake);
		}
	}
}

void PlayerMovement::CollideAndSlide(const Vec3& desiredDisplacement) {
	auto* collider = owner_->GetComponent<BoxColliderComponent>();
	if (!collider) {
		position_ += desiredDisplacement;
		return;
	}

	const int       kMaxBounces   = 16;      // 最大反射回数
	constexpr float kEpsilon      = 0.0075f; // 衝突判定の許容値
	constexpr float kPushOut      = 0.015f;  // 押し出し量
	const float     stepMaxHeight = 0.3f;    // 床とみなす最大段差(必要に応じて調整)

	Vec3 remainingDisp = desiredDisplacement;
	Vec3 currentPos    = transform_->GetWorldPos() + collider->GetOffset();
	Vec3 finalVelocity = velocity_;
	Vec3 averageNormal = Vec3::zero;
	int  hitCount      = 0;

	for (int bounce = 0; bounce < kMaxBounces && remainingDisp.SqrLength() >
	     kEpsilon * kEpsilon; ++bounce) {
		float moveLength = remainingDisp.Length();
		auto  hitResults = collider->BoxCast(currentPos,
		                                     remainingDisp.Normalized(),
		                                     moveLength,
		                                     collider->GetBoundingBox().
		                                     GetHalfSize());

		if (hitResults.empty()) {
			currentPos += remainingDisp;
			break;
		}

		// エッジ衝突の検出と平均法線の計算
		averageNormal = Vec3::zero;
		hitCount      = 0;
		for (const auto& result : hitResults) {
			if (result.dist < kEpsilon * 2.0f) {
				averageNormal += result.hitNormal;
				hitCount++;
			}
		}

		auto hit = *std::ranges::min_element(hitResults,
		                                     [](const HitResult& a,
		                                        const HitResult& b) {
			                                     return a.dist < b.dist;
		                                     });

		Vec3 hitNormal = hit.hitNormal;
		if (hitCount > 1) {
			// 複数の法線がある場合は平均化して正規化
			hitNormal = (averageNormal / static_cast<float>(hitCount)).
				Normalized();
		}

		// エッジケースの検出
		bool isEdgeCollision = false;
		if (hitCount > 1 && std::abs(hit.hitNormal.Dot(hitNormal)) <
			kEdgeAngleThreshold) {
			isEdgeCollision = true;
			// エッジに沿った移動ベクトルを計算
			Vec3 edgeDir  = hit.hitNormal.Cross(hitNormal).Normalized();
			remainingDisp = edgeDir * remainingDisp.Dot(edgeDir);
		} else {
			float travelDist = (std::max)(hit.dist - kEpsilon, 0.0f);
			currentPos += remainingDisp.Normalized() * travelDist;

			if (hit.dist < kEpsilon) {
				currentPos += hitNormal * kPushOut;
			}

			float fraction = hit.dist / moveLength;
			remainingDisp  = remainingDisp * (1.0f - fraction);

			Vec3 slide = remainingDisp - hitNormal * remainingDisp.Dot(
				hitNormal);

			// 接地判定：接触点の高さが現在位置から stepMaxHeight 以内の場合のみ床とみなす
			if (hitNormal.y > 0.7f && hit.hitPos.y <= currentPos.y +
				stepMaxHeight) {
				remainingDisp = slide;
				mGroundNormal = hitNormal;
				bIsGrounded   = true;
			} else {
				Vec3 reflection = remainingDisp - 2.0f * hitNormal *
					remainingDisp.Dot(hitNormal);
				remainingDisp = slide * 0.8f + reflection * 0.2f;
				finalVelocity = finalVelocity - hitNormal * finalVelocity.Dot(
					hitNormal);
			}
		}

		// // エッジ衝突時の速度調整
		// if (isEdgeCollision) {
		// 	finalVelocity *= 0.8f; // エッジ衝突時は速度を減衰
		// }
	}

	position_ = currentPos - collider->GetOffset();
	velocity_ = finalVelocity;
}
