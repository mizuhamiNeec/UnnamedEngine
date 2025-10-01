#pragma once
#include <engine/public/Components/base/Component.h>

class SceneComponent;

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
	float mPitch = 0.0f;
	float mYaw   = 0.0f;

	bool isMouseLocked_ = true;
};
