#pragma once
#include <Lib/Math/Vector/Vec3.h>
#include <Components/Base/Component.h>

class TransformComponent;

//-----------------------------------------------------------------------------
// 基本的にカメラを回転させるコンポーネントです。
//-----------------------------------------------------------------------------
class CameraRotator : public Component {
public:
	~CameraRotator() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

private:
	TransformComponent* transform_ = nullptr;
	float pitch_ = 0.0f;
	float yaw_ = 0.0f;

	bool isMouseLocked_ = true;
};

