#pragma once
#include <engine/Components/base/Component.h>

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
	
	// Animation offsets (added by CameraAnimator or other components)
	float mAnimationPitchOffset = 0.0f; // degrees
	float mAnimationRollOffset = 0.0f;  // degrees

public:
	// Setters for animation offsets
	void SetAnimationPitchOffset(float pitch) { mAnimationPitchOffset = pitch; }
	void SetAnimationRollOffset(float roll) { mAnimationRollOffset = roll; }
};
