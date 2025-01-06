#pragma once
#include <Lib/Math/Vector/Vec3.h>
#include <Components/Base/Component.h>

class TransformComponent;

class CameraRotator : public Component {
public:
	~CameraRotator() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

private:
	TransformComponent* transform_ = nullptr;
	Vec3 rot_ = Vec3::zero;

	bool isMouseLocked_ = true;
};

