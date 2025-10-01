#pragma once

#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

class CameraComponent : public Component {
public:
	~CameraComponent() override;

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	float& GetFovVertical();
	void   SetFovVertical(const float& newFovVertical);

	float& GetZNear();
	void   SetNearZ(const float& newNearZ);

	float& GetZFar();
	void   SetFarZ(const float& newFarZ);

	Mat4& GetViewProjMat();
	Mat4& GetViewMat();
	Mat4& GetProjMat();

	float& GetAspectRatio();
	void   SetAspectRatio(float newAspectRatio);
	void   SetViewMat(const Mat4& mat4);

private:
	float mFov         = 90.0f * Math::deg2Rad;
	float mAspectRatio = 0.0f;
	float mZNear       = 0x.1p1f;
	float mZFar        = 0x61A8p0f;

	Mat4 mWorldMat;
	Mat4 mViewMat;
	Mat4 mProjMat;
	Mat4 mViewProjMat;
};
