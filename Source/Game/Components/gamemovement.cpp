#include "gamemovement.h"

#include <algorithm>

#include "../../Engine/EntityComponentSystem/Entity/Base/BaseEntity.h"
#include "../Debug/Debug.h"
#include "../../Engine/Lib/Console/Console.h"

#include "../../Engine/EntityComponentSystem/Components/Camera/CameraComponent.h"
#include "../../Engine/Input/InputSystem.h"
#include "../../Engine/Lib/Console/ConVarManager.h"

Gamemovement::Gamemovement(BaseEntity* owner) : BaseComponent(owner) {}

Gamemovement::~Gamemovement() = default;

void Gamemovement::Initialize() {
	transformComponent_ = parentEntity_->GetComponent<TransformComponent>();
}

void Gamemovement::Update(const float deltaTime) {
	ProcessMovement(deltaTime);
}

void Gamemovement::Terminate() {}

void Gamemovement::Serialize([[maybe_unused]] std::ostream& out) const {}

void Gamemovement::Deserialize([[maybe_unused]] std::istream& in) {}

void Gamemovement::ImGuiDraw() {}

void Gamemovement::ProcessMovement([[maybe_unused]] float deltaTime) {
	this->deltaTime_ = deltaTime;
	/*Console::Print(std::format(
		"start : {} {} {}\n",
		transformComponent_->GetWorldPosition().x,
		transformComponent_->GetWorldPosition().y,
		transformComponent_->GetWorldPosition().z
	), kConsoleColorWait);*/

	PlayerMove();

	FinishMove();

	static float radius = 0.5f;
	Debug::DrawCapsule(
		transformComponent_->GetWorldPosition(),
		transformComponent_->GetWorldRotation(),
		2.0f,
		radius,
		{ 0.0f,1.0f,1.0f,1.0f }
	);

	Debug::DrawArrow(
		transformComponent_->GetWorldPosition(),
		vecVel_,
		{ 1.0f,1.0f,0.0f,1.0f },
		0.25f
	);

#ifdef _DEBUG
	ImGui::Begin("gamemovement");
	ImGui::Text("outwishvel : %f %f %f", outWishVel.x, outWishVel.y, outWishVel.z);
	ImGui::Text("velocity : %f %f %f", vecVel_.x, vecVel_.y, vecVel_.z);
	ImGui::Text("position : %f %f %f", transformComponent_->GetWorldPosition().x, transformComponent_->GetWorldPosition().y, transformComponent_->GetWorldPosition().z);
	ImGui::Text("onGround : %d", onGround);
	ImGui::Text("forward : %f %f %f", forward.x, forward.y, forward.z);
	ImGui::Text("right : %f %f %f", right.x, right.y, right.z);
	ImGui::Text("up : %f %f %f", up.x, up.y, up.z);
	ImGui::End();
#endif

	// 速度を適用して位置を更新
	Vec3 newPosition = transformComponent_->GetWorldPosition() + vecVel_ * deltaTime;
	transformComponent_->SetWorldPos(newPosition);

	/*Console::Print(std::format(
		"end : {} {} {}\n",
		transformComponent_->GetWorldPosition().x,
		transformComponent_->GetWorldPosition().y,
		transformComponent_->GetWorldPosition().z
	), kConsoleColorCompleted);*/
}

void Gamemovement::PlayerMove() {
	outWishVel = Vec3::zero;
	outJumpVel = Vec3::zero;

	//Quaternion camRot = Quaternion::identity;

	right = Vec3::right;
	up = Vec3::up;
	forward = Vec3::forward;

	FullWalkMove();
}

void Gamemovement::FinishMove() {}

void Gamemovement::FullWalkMove() {
	StartGravity();

	if (onGround) {
		vecVel_.y = 0.0f;
		Friction();
	}

	CheckVelocity();

	if (onGround) {
		WalkMove();
	} else {
		AirMove();
	}

	CheckVelocity();

	FinishGravity();

	if (onGround) {
		vecVel_.y = 0.0f;
	}
}

void Gamemovement::WalkMove() {
	Vec3 wishvel;
	//float spd;
	Vec2 move;
	Vec3 wishdir;
	float wishspeed;

	Vec3 dest;

	move = Vec2::zero;

	if (InputSystem::IsPressed("forward")) {
		move.y += 1.0f;
	}

	if (InputSystem::IsPressed("back")) {
		move.y -= 1.0f;
	}

	if (InputSystem::IsPressed("moveright")) {
		move.x += 1.0f;
	}

	if (InputSystem::IsPressed("moveleft")) {
		move.x -= 1.0f;
	}

	move.Normalize();

	move *= 320.0f;

	if (forward.y != 0) {
		forward.y = 0;
		forward.Normalize();
	}

	if (right.y != 0) {
		right.y = 0;
		right.Normalize();
	}

	wishvel = forward * move.y + right * move.x;

	wishvel.y = 0.0f;

	wishdir = wishvel;
	wishspeed = wishdir.Length();

	float maxspeed = std::stof(ConVarManager::GetConVar("sv_maxspeed")->GetValueAsString());

	if ((wishspeed != 0.0f) && (wishspeed > maxspeed)) {
		wishvel *= maxspeed / wishspeed;
		wishspeed = maxspeed;
	}

	vecVel_.y = 0;
	Accelerate(wishdir, wishspeed, std::stof(ConVarManager::GetConVar("sv_accelerate")->GetValueAsString()));
	vecVel_.y = 0;
}

void Gamemovement::Friction() {
	// Calculate speed
	float speed = vecVel_.Length();

	// If too slow, return
	if (speed < 0.01f) {
		return;
	}

	float drop = 0.0f;

	if (onGround) {
		float friction = std::stof(ConVarManager::GetConVar("sv_friction")->GetValueAsString()); // TODO surface Friction
		float stopSpeed = std::stof(ConVarManager::GetConVar("sv_stopspeed")->GetValueAsString());
		float control = (speed < stopSpeed) ? stopSpeed : speed;
		drop += control * friction * deltaTime_;
	}

	float newspeed = speed - drop;

	newspeed = std::max<float>(newspeed, 0);

	if (newspeed != speed) {
		newspeed /= speed;
		vecVel_ *= newspeed;
	}

	vecVel_ -= (1.0f - newspeed) * vecVel_;
}

void Gamemovement::Accelerate(Vec3 wishdir, float wishspeed, float accel) {
	float addspeed, accelspeed, currentspeed;

	currentspeed = vecVel_.Dot(wishdir);

	addspeed = wishspeed - currentspeed;

	if (addspeed <= 0) {
		return;
	}

	accelspeed = accel * deltaTime_ * wishspeed; // TODO Surface friction

	if (accelspeed > addspeed) {
		accelspeed = addspeed;
	}

	vecVel_ += accelspeed * wishdir;
}

void Gamemovement::AirMove() {

}

void Gamemovement::StartGravity() {

	float gravity = std::stof(
		ConVarManager::GetConVar("sv_gravity")->GetValueAsString()
	);

	vecVel_.y -= gravity * 0.5f * deltaTime_;
	// TODO

	CheckVelocity();
}

void Gamemovement::FinishGravity() {
	float gravity = std::stof(
		ConVarManager::GetConVar("sv_gravity")->GetValueAsString()
	);

	vecVel_.y -= gravity * 0.5f * deltaTime_;

	CheckVelocity();
}

void Gamemovement::CheckVelocity() {
	Vec3 org = transformComponent_->GetWorldPosition();

	for (int i = 0; i < 3; ++i) {
		if (std::isnan(vecVel_[i])) {
			Console::Print(std::format("PM  Got a NaN velocity {}\n", DescribeAxis(i)), kConsoleColorError);
			vecVel_[i] = 0.0f;
		}

		if (std::isnan(org[i])) {
			Console::Print(std::format("PM  Got a NaN origin on {}\n", DescribeAxis(i)), kConsoleColorError);
			org[i] = 0.0f;
			transformComponent_->SetWorldPos(org);
		}

		float maxVel = std::stof(ConVarManager::GetConVar("sv_maxvelocity")->GetValueAsString());

		if (vecVel_[i] > maxVel) {
			Console::Print(std::format("PM  Got a velocity too high on {}\n", DescribeAxis(i)), kConsoleColorError);
			vecVel_[i] = maxVel;
		} else if (vecVel_[i] < -maxVel) {
			Console::Print(std::format("PM  Got a velocity too low on {}\n", DescribeAxis(i)), kConsoleColorError);
			vecVel_[i] = -vecVel_[i];
		}
	}
}

std::string Gamemovement::DescribeAxis(const int& i) {
	switch (i) {
	case 0:
		return "X";
	case 1:
		return "Y";
	case 2:
		return "Z";
	default:
		return "Unknown";
	}
}

