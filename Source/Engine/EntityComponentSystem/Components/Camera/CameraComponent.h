#pragma once
#include "../EntityComponentSystem/Components/Base/BaseComponent.h"

#include "../Lib/Structs/Structs.h"
#include "../Lib/Math/MathLib.h"

class CameraComponent : public BaseComponent {
public:
	CameraComponent(BaseEntity* owner);

	// BaseComponent
	void Initialize() override;
	void Update(float deltaTime) override;
	void Terminate() override;
	void Serialize(std::ostream& out) const override;
	void Deserialize(std::istream& in) override;
	void ImGuiDraw() override;

	// Setter
	void SetFovVertical(const float& newFovVertical);
	void SetNearZ(const float& newNearZ);
	void SetFarZ(const float& newFarZ);
	void SetAspectRatio(float newAspectRatio);

	// Getter
	float& GetFovVertical();
	float& GetZNear();
	float& GetZFar();
	float& GetAspectRatio();
	Mat4& GetViewProjMat();
	[[nodiscard]] Mat4 GetViewMat() const;
	[[nodiscard]] Mat4 GetProjMat() const;

private:
	float fov_ = 90.0f * Math::deg2Rad;
	float aspectRatio_ = 16.0f / 9.0f;
	float zNear_ = 0.1f;
	float zFar_ = 10000.0f;

	Mat4 viewProjMat_;

	void UpdateViewProjMat();
};
