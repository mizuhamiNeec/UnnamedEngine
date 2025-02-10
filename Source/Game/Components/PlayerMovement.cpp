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
#include "Lib/DebugHud/DebugHud.h"

#include "Physics/PhysicsEngine.h"

PlayerMovement::~PlayerMovement() {}

void PlayerMovement::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	// 初期化処理
	// トランスフォームコンポーネントを取得
	transform_ = owner_->GetTransform();

	speed_ = 300.0f;
	jumpVel_ = 300.0f;

	// Console::SubmitCommand("sv_gravity 0");
}

void PlayerMovement::ProcessInput() {
	//-------------------------------------------------------------------------
	// 移動入力
	//-------------------------------------------------------------------------
	moveInput_ = {0.0f,0.0f,0.0f};

	if(InputSystem::IsPressed("forward")) {
		moveInput_.z += 1.0f;
	}

	if(InputSystem::IsPressed("back")) {
		moveInput_.z -= 1.0f;
	}

	if(InputSystem::IsPressed("moveright")) {
		moveInput_.x += 1.0f;
	}

	if(InputSystem::IsPressed("moveleft")) {
		moveInput_.x -= 1.0f;
	}

	moveInput_.Normalize();
}

void PlayerMovement::Update([[maybe_unused]] const float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	// 入力処理
	ProcessInput();

	Move();

	// position_.y = std::max<float>(position_.y, 0.0f);
	transform_->SetLocalPos(position_);
	// デバッグ描画
	Debug::DrawArrow(transform_->GetWorldPos(),velocity_,Vec4::yellow);

	float width = Math::HtoM(33.0f);
	float height = Math::HtoM(73.0f);
	Debug::DrawBox(
		transform_->GetWorldPos() + (Vec3::up * height * 0.5f),
		Quaternion::Euler(Vec3::zero),
		Vec3(width,height,width),
		isGrounded_ ? Vec4::green : Vec4::blue);
	Debug::DrawRay(transform_->GetWorldPos(),wishdir_,Vec4::cyan);

	//// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	// auto camera = CameraManager::GetActiveCamera();
	// Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();

	// auto* collider = owner_->GetComponent<ColliderComponent>();

	// if (!collider) {
	//	Console::Print(
	//		"PlayerMovement::CollideAndSlide() : ColliderComponent is not attached to the owner entity.",
	//		Vec4::yellow,
	//		Channel::Physics
	//	);
	// } else {
	//	std::vector<HitResult> hits = collider->RayCast(camera->GetViewMat().Inverse().GetTranslate(), cameraForward, 100.0f);

	//	Debug::DrawRay(camera->GetViewMat().Inverse().GetTranslate(), cameraForward * 100.0f, Vec4::red);
	//	ImGui::Begin("Hit", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	//	ImDrawList* drawList = ImGui::GetWindowDrawList();
	//	float outlineSize = 1.0f;
	//	for (auto hit : hits) {
	//		if (hit.isHit) {
	//			Debug::DrawBox(hit.hitPos, Quaternion::identity, Vec3(0.1f, 0.1f, 0.1f), Vec4::green);
	//			Debug::DrawRay(hit.hitPos, hit.hitNormal, Vec4::magenta);

	//			static bool isOffscreen;
	//			static float outAngle;

	//			Vec2 scrPos = Math::WorldToScreen(hit.hitPos, false, 32.0f, isOffscreen, outAngle);

	//			if (!isOffscreen) {
	//				std::string text = "HitPos: " + hit.hitPos.ToString() + "\nHitNormal: " + hit.hitNormal.ToString();

	//				ImGuiManager::TextOutlined(
	//					drawList,
	//					{ scrPos.x,scrPos.y },
	//					text.c_str(),
	//					ToImVec4(kConFgColorDark),
	//					ToImVec4(kDebugHudOutlineColor),
	//					outlineSize
	//				);
	//			}
	//		}
	//	}
	//	ImGui::End();
	//}
}

static Vec3 test;

static float kEdgeAngleThreshold = 0.2f; // エッジ判定の閾値

void PlayerMovement::DrawInspectorImGui() {
	#ifdef _DEBUG
	if(ImGui::CollapsingHeader("PlayerMovement",ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGuiManager::DragVec3("Velocity",velocity_,0.1f,"%.2f m/s");
		ImGuiManager::DragVec3("test",test,0.1f,"%.2f m/s");
		ImGui::DragFloat("edge angle threshold",&kEdgeAngleThreshold,0.01f,0.0f,1.0f);
	}
	#endif
}

void PlayerMovement::Move() {
	if(!isGrounded_) {
		ApplyHalfGravity();
	}

	isGrounded_ = CheckGrounded();

	// ジャンプ処理
	if(isGrounded_ && InputSystem::IsPressed("+jump")) {
		velocity_.y = Math::HtoM(jumpVel_);
		isGrounded_ = false; // ジャンプ中は接地判定を解除
	}

	// if (isGrounded_ && velocity_.y < 0.0f) {
	//	velocity_.y = 0.0f; // 落下中のみリセット
	// }

	if(isGrounded_) {
		ApplyFriction();
	}

	// CheckVelocity
	CheckVelocity();

	// カメラの前方ベクトルを取得し、XZ平面に投影して正規化
	auto camera = CameraManager::GetActiveCamera();
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();

	cameraForward.y = 0.0f; // Y成分を0にする
	cameraForward.Normalize();

	// カメラの右方向ベクトルを計算（Y軸との外積）
	Vec3 cameraRight = Vec3::up.Cross(cameraForward).Normalized();

	// 入力に基づいて移動方向を計算
	Vec3 wishvel = (cameraForward * moveInput_.z) + (cameraRight * moveInput_.x);
	wishvel.y = 0.0f;

	if(!wishvel.IsZero()) {
		wishvel.Normalize();
	}

	if(isGrounded_) {
		wishdir_ = wishvel;
		float wishspeed = wishdir_.Length() * speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if(wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		Accelerate(wishdir_,wishspeed,ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat());

		// 地面にいたらベロシティを地面の法線方向に投影
		velocity_ = velocity_ - (normal_ * velocity_.Dot(normal_));
	} else {
		wishdir_ = wishvel;
		float wishspeed = wishdir_.Length() * speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if(wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
		}
		AirAccelerate(wishdir_,wishspeed,ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat());
	}

	CollideAndSlide(velocity_ * deltaTime_);

	if(!isGrounded_) {
		ApplyHalfGravity();
	}

	CheckVelocity();
}

void PlayerMovement::SetIsGrounded(bool bIsGrounded) {
	isGrounded_ = bIsGrounded;
}

void PlayerMovement::SetVelocity(const Vec3 newVel) {
	velocity_ = newVel;
}

void PlayerMovement::CollideAndSlide(const Vec3& desiredDisplacement) {
	auto* collider = owner_->GetComponent<BoxColliderComponent>();
	if(!collider) {
		position_ += desiredDisplacement;
		return;
	}

	const int kMaxBounces = 4;		  // 最大反射回数
	const float kEpsilon = 0.001f;	  // 衝突判定の許容値
	const float kPushOut = 0.005f;	  // 押し出し量
	const float stepMaxHeight = 0.3f; // 床とみなす最大段差(必要に応じて調整)

	Vec3 remainingDisp = desiredDisplacement;
	Vec3 currentPos = transform_->GetWorldPos() + collider->GetOffset();
	Vec3 finalVelocity = velocity_;
	Vec3 averageNormal = Vec3::zero;
	int hitCount = 0;

	for(int bounce = 0; bounce < kMaxBounces && remainingDisp.SqrLength() > kEpsilon * kEpsilon; ++bounce) {
		float moveLength = remainingDisp.Length();
		auto hitResults = collider->BoxCast(currentPos,remainingDisp.Normalized(),moveLength,
			collider->GetBoundingBox().GetHalfSize());

		if(hitResults.empty()) {
			currentPos += remainingDisp;
			break;
		}

		// エッジ衝突の検出と平均法線の計算
		averageNormal = Vec3::zero;
		hitCount = 0;
		for(const auto& result : hitResults) {
			if(result.dist < kEpsilon * 2.0f) {
				averageNormal += result.hitNormal;
				hitCount++;
			}
		}

		auto hit = *std::ranges::min_element(hitResults,[](const HitResult& a,const HitResult& b) { return a.dist < b.dist; });

		Vec3 hitNormal = hit.hitNormal;
		if(hitCount > 1) {
			// 複数の法線がある場合は平均化して正規化
			hitNormal = (averageNormal / static_cast<float>(hitCount)).Normalized();
		}

		// エッジケースの検出
		bool isEdgeCollision = false;
		if(hitCount > 1 && std::abs(hit.hitNormal.Dot(hitNormal)) < kEdgeAngleThreshold) {
			isEdgeCollision = true;
			// エッジに沿った移動ベクトルを計算
			Vec3 edgeDir = hit.hitNormal.Cross(hitNormal).Normalized();
			remainingDisp = edgeDir * remainingDisp.Dot(edgeDir);
		} else {
			float travelDist = (std::max)(hit.dist - kEpsilon,0.0f);
			currentPos += remainingDisp.Normalized() * travelDist;

			if(hit.dist < kEpsilon) {
				currentPos += hitNormal * kPushOut;
			}

			float fraction = hit.dist / moveLength;
			remainingDisp = remainingDisp * (1.0f - fraction);

			Vec3 slide = remainingDisp - hitNormal * remainingDisp.Dot(hitNormal);

			// 接地判定：接触点の高さが現在位置から stepMaxHeight 以内の場合のみ床とみなす
			if(hitNormal.y > 0.7f && hit.hitPos.y <= currentPos.y + stepMaxHeight) {
				remainingDisp = slide;
				normal_ = hitNormal;
				isGrounded_ = true;
			} else {
				Vec3 reflection = remainingDisp - 2.0f * hitNormal * remainingDisp.Dot(hitNormal);
				remainingDisp = slide * 0.8f + reflection * 0.2f;
				finalVelocity = finalVelocity - hitNormal * finalVelocity.Dot(hitNormal);
			}
		}

		// エッジ衝突時の速度調整
		if(isEdgeCollision) {
			finalVelocity *= 0.8f; // エッジ衝突時は速度を減衰
		}
	}

	position_ = currentPos - collider->GetOffset();
	velocity_ = finalVelocity;
}
