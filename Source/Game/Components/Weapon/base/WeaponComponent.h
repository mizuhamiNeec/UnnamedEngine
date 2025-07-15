#pragma once
#include <memory>
#include <string>
#include <Components/Base/Component.h>

#include "Lib/Math/Vector/Vec3.h"

class Entity;

struct WeaponData {
	std::string name;
	int         maxAmmo    = 0;         // 最大弾薬数
	int         clipSize   = 0;         // クリップサイズ
	float       fireRate   = 0.0f;      // 発射レート [秒あたりの発射数]
	float       reloadTime = 0.0f;      // リロード時間 [秒]
	float       damage     = 0.0f;      // 攻撃力
	std::string primaryModule;          // モジュール名
	std::string secondaryModule;        // モジュール名
	float       projectileSpeed = 0.0f; // 弾丸の速度 [HU/s]

	static std::unique_ptr<WeaponData>
	LoadFromJson(const std::string& jsonPath);
};

class IWeaponModule {
public:
	virtual ~IWeaponModule() = default;

	virtual void Execute(Entity& entity) = 0;
	virtual void Update(const float& deltaTime) = 0;
	virtual void DrawInspectorImGui() = 0;
};

class HitscanModule final : public IWeaponModule {
public:
	explicit HitscanModule(const WeaponData& weaponData) :
		data_(weaponData) {
	}

	void Execute(Entity& entity) override;

	void Update(const float& deltaTime) override;

	void DrawInspectorImGui() override;

	[[nodiscard]] const WeaponData& GetWeaponData() const { return data_; }

	Vec3 GetHitPosition() const {
		if (bHit_) {
			return hitPosition_;
		}
		return Vec3::min; // ヒットしていない場合はあらぬ座標を返す
	}

	Vec3& GetHitNormal() {
		if (bHit_) {
			return hitNormal_;
		}
		return hitNormal_;
	}

private:
	const WeaponData& data_;

	Vec3 hitPosition_ = Vec3::zero; // ヒット位置
	Vec3 hitNormal_   = Vec3::zero; // ヒット面の法線
	bool bHit_        = false;      // ヒットしたかどうか
};

// //-----------------------------------------------------------------------------
// // 近接攻撃
// //-----------------------------------------------------------------------------
// class MeleeModule final : public IWeaponModule {
// public:
// 	explicit MeleeModule(
// 		const float& range, const float& damage) :
// 		mRange(range),
// 		mDamage(damage) {
// 	}
//
// 	~MeleeModule() override;
// 	void Execute(Entity& entity) override;
// 	void Update(const float& deltaTime) override;
// 	void DrawInspectorImGui() override;
//
// private:
// 	float mRange;  // 攻撃範囲 [HU]
// 	float mDamage; // 攻撃力
// };


class WeaponComponent : public Component {
public:
	~WeaponComponent() override;

	explicit WeaponComponent(const std::string& weaponJsonPath);

	void OnAttach(Entity& owner) override;

	void Update(float deltaTime) override;
	//void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	void PullTrigger();
	void ReleaseTrigger();
	void Reload();
	bool CanFire() const;

	Vec3  GetHitPosition() const;
	Vec3& GetHitNormal();

	[[nodiscard]] bool HasFiredThisFrame() const;

	[[nodiscard]] Entity* GetOwner() const override;

	void LoadFromJson(const std::string& jsonPath);

private:
	struct ActionMapping {
		std::unique_ptr<IWeaponModule> primaryAction;
		std::unique_ptr<IWeaponModule> secondaryAction;
	} mAction;

	std::unique_ptr<WeaponData>    mWeaponData;
	std::unique_ptr<IWeaponModule> mPrimaryModule; // 主モジュール

	Vec3 mHitPosition = Vec3::zero; // ヒット位置

	int   mCurrentAmmo   = 0;
	int   mCurrentClip   = 0;
	float timeSinceShot_ = 0.0f; // 最後に発射してからの時間

	bool  bIsReloading_ = false; // リロード中かどうか
	float reloadTimer_  = 0.0f;  // リロードタイマー

	bool bTriggerHeld_ = false; // トリガーが押されているかどうか

	bool bFiredThisFrame_ = false; // 今フレームで発射したかどうか
};
