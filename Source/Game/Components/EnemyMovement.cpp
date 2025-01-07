#include "EnemyMovement.h"

#include "Debug/Debug.h"
#include "Entity/Base/Entity.h"
#include "Lib/Console/ConVarManager.h"

EnemyMovement::~EnemyMovement() {}

void EnemyMovement::OnAttach(Entity& owner) {
	CharacterMovement::OnAttach(owner);
	transform_ = owner_->GetTransform();
	speed_ = 100.0f;
}

void EnemyMovement::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	position_ = transform_->GetLocalPos();

	Move();

	transform_->SetLocalPos(position_ + velocity_ * deltaTime_);

	// 各軸の範囲外に出たら制限し、各軸のvelocityを0にする
	Vec3 worldPos = transform_->GetWorldPos();

	if (worldPos.x > 32.0f || worldPos.x < -32.0f) {
		worldPos.x = std::clamp(worldPos.x, -32.0f, 32.0f);
		velocity_.x = 0.0f;
	}

	if (worldPos.z > 32.0f || worldPos.z < -32.0f) {
		worldPos.z = std::clamp(worldPos.z, -32.0f, 32.0f);
		velocity_.z = 0.0f;
	}

	transform_->SetWorldPos(worldPos);

	// 範囲外に出ないように制限
	transform_->SetWorldPos(transform_->GetWorldPos().Clamp(Vec3(-32.0f, 0.0f, -32.0f), Vec3(32.0f, 100.0f, 32.0f)));

	Debug::DrawArrow(transform_->GetWorldPos(), velocity_ * 0.25f, Vec4::yellow);
	Debug::DrawCapsule(transform_->GetWorldPos(), Quaternion::Euler(Vec3::zero), 2.0f, 0.5f, isGrounded_ ? Vec4::green : Vec4::red);
}

void EnemyMovement::DrawInspectorImGui() {
	CharacterMovement::DrawInspectorImGui();
}

void EnemyMovement::Move() {
	// 接地判定
	isGrounded_ = CheckGrounded();

	if (!isGrounded_) {
		ApplyHalfGravity();
	}

	if (isGrounded_) {
		ApplyFriction();
	}

	// CheckVelocity
	CheckVelocity();

	if (isGrounded_) {
		velocity_.y = 0.0f;
		Vec3 wishvel = moveInput_;
		wishvel.y = 0.0f;
		wishvel.Normalize();
		Vec3 wishdir = wishvel;
		float wishspeed = wishdir.Length();
		wishspeed *= speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if ((wishspeed != 0.0f) && wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
			wishspeed = maxspeed;
		}
		Accelerate(wishdir, wishspeed, ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat());
	} else {
		Vec3 wishvel = moveInput_;
		wishvel.y = 0.0f;
		Vec3 wishdir = wishvel;
		wishvel.Normalize();
		float wishspeed = wishdir.Length();
		wishspeed *= speed_;
		float maxspeed = ConVarManager::GetConVar("sv_maxspeed")->GetValueAsFloat();
		if ((wishspeed != 0.0f) && wishspeed > maxspeed) {
			wishvel *= maxspeed / wishspeed;
			wishspeed = maxspeed;
		}
		AirAccelerate(wishdir, wishspeed, ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat());
	}

	// y座標が0以下の場合は0にする
	position_.y = max(position_.y, 0.0f);

	if (!isGrounded_) {
		ApplyHalfGravity();
	}
}

void EnemyMovement::SetMoveInput(const Vec3 newMoveInput) {
	moveInput_ = newMoveInput.Normalized();
}

void EnemyMovement::OnCollisionWithEnemy(Entity* otherEnemy) {
	Vec3 currentPos = owner_->GetTransform()->GetWorldPos();
	Vec3 otherPos = otherEnemy->GetTransform()->GetWorldPos();
	Vec3 dir = (currentPos - otherPos).Normalized();
	Vec3 targetPos = currentPos + dir * 0.1f;

	// 線形補間を使用してスムーズに位置を更新
	float lerpFactor = 0.1f; // 補間係数（0.0fから1.0fの範囲）
	Vec3 newPos = Math::Lerp(currentPos, targetPos, lerpFactor);

	owner_->GetTransform()->SetLocalPos(newPos);
}

void EnemyMovement::OnSwordHit(Entity* enemy) {
	Vec3 currentPos = owner_->GetTransform()->GetWorldPos();
	Vec3 enemyPos = enemy->GetTransform()->GetWorldPos();
	Vec3 knockbackDir = (currentPos - enemyPos).Normalized();
	knockbackDir.y = 0.5f; // 上方向の成分を追加
	knockbackDir.Normalize(); // 正規化して方向ベクトルを得る
	float knockbackStrength = 1.0f; // ノックバックの強さを調整

	Vec3 knockbackVelocity = knockbackDir * knockbackStrength;
	owner_->GetComponent<EnemyMovement>()->ApplyKnockBack(knockbackVelocity);
}

void EnemyMovement::ApplyKnockBack(const Vec3& knockBackVel) {
	velocity_ += knockBackVel * 5.0f; // ノックバックの強さを調整
	transform_->SetWorldPos(transform_->GetWorldPos() + Vec3::up * 0.25f);
	isGrounded_ = false;
}
