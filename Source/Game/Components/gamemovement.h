#pragma once

#include "../../Engine/EntityComponentSystem/Components/Base/BaseComponent.h"
#include "../../Engine/Lib/Math/Vector/Vec3.h"

class CameraComponent;
class TransformComponent;

class Gamemovement : public BaseComponent {
public:
	explicit Gamemovement(BaseEntity* owner);
	~Gamemovement() override;
	void Initialize() override;
	void Update(float deltaTime) override;
	void Terminate() override;
	void Serialize(std::ostream& out) const override;
	void Deserialize(std::istream& in) override;
	void ImGuiDraw() override;

private:
	void ProcessMovement(float deltaTime);
	void PlayerMove();
	void FinishMove();

	void FullWalkMove();
	void WalkMove();
	void Friction();
	void Accelerate(Vec3 wishdir, float wishspeed, float accel);
	void AirMove();

	void StartGravity();
	void FinishGravity();

	void CheckVelocity();

	static std::string DescribeAxis(const int& i);

	TransformComponent* transformComponent_ = nullptr;
	TransformComponent* cameraTransformComponent_ = nullptr;

	Vec3 vecVel_ = Vec3::zero;

	Vec3 outWishVel = Vec3::zero;
	Vec3 outJumpVel = Vec3::zero;

	Vec3 up = Vec3::up;
	Vec3 right = Vec3::right;
	Vec3 forward = Vec3::forward;

	bool onGround = true;

	float deltaTime_ = 0.0f;
};

