#pragma once
#include <memory>
#include <string>
#include <Components/Base/Component.h>

class Entity;

class IWeaponModule {
public:
	virtual ~IWeaponModule() = default;

	virtual void Execute(Entity& entity) = 0;
	virtual void Update(const float& deltaTime) = 0;
	virtual void DrawInspectorImGui() = 0;
};

//-----------------------------------------------------------------------------
// 近接攻撃
//-----------------------------------------------------------------------------
class MeleeModule final : public IWeaponModule {
public:
	explicit MeleeModule(
		const float& range, const float& damage) :
		mRange(range),
		mDamage(damage) {
	}

	~MeleeModule() override;
	void Execute(Entity& entity) override;
	void Update(const float& deltaTime) override;
	void DrawInspectorImGui() override;

private:
	float mRange;  // 攻撃範囲 [HU]
	float mDamage; // 攻撃力
};


class WeaponComponent : public Component {
public:
	~WeaponComponent() override;

	void OnAttach(Entity& owner) override;

	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	[[nodiscard]] bool    IsEditorOnly() const override { return false; }
	[[nodiscard]] Entity* GetOwner() const override;

	void PrimaryAction();
	void SecondaryAction();

	void LoadFromJson(const std::string& jsonPath);

private:
	struct ActionMapping {
		std::unique_ptr<IWeaponModule> primaryAction;
		std::unique_ptr<IWeaponModule> secondaryAction;
	} mAction;

	// UI用
	int  mCurrentAmmo = 0;
	bool bIsBusy      = false; // リロード、クールダウンなど
};
