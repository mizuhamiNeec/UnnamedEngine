#pragma once

#include <engine/Components/Base/Component.h>

//-----------------------------------------------------------------------------
// ViewModelを揺らすやつ
//-----------------------------------------------------------------------------
class WeaponSway final : public Component {
public:
	WeaponSway(const float& swayAmount = 0.025f) : mSwayAmount(swayAmount) {
	}

	~WeaponSway() override;

	void OnAttach(Entity& owner) override;

	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	[[nodiscard]] Entity* GetOwner() const override;

private:
	float mSwayAmount  = 0.0f; // 揺らす量
	float mPitch       = 0.0f;
	float mYaw         = 0.0f;
	float mAttenuation = 8.0f;
};
