#include "WeaponComponent.h"

#include <fstream>

#define NOMINMAX

#include "Camera/CameraManager.h"
#include "Components/Camera/CameraComponent.h"
#include "Lib/Math/Vector/Vec3.h"
#include "Physics/PhysicsEngine.h"

#include <nlohmann/json.hpp>

#include "Components/ColliderComponent/BoxColliderComponent.h"
#include "Components/ColliderComponent/Base/ColliderComponent.h"
#include "Debug/Debug.h"

using json = nlohmann::json;

std::unique_ptr<WeaponData> WeaponData::LoadFromJson(
	const std::string& jsonPath) {
	std::ifstream file(jsonPath);
	json          j;
	file >> j;

	auto d             = std::make_unique<WeaponData>();
	d->name            = j["name"].get<std::string>();
	d->maxAmmo         = j["max_ammo"];
	d->clipSize        = j["clip_size"];
	d->fireRate        = j["fire_rate"];
	d->reloadTime      = j["reload_time"];
	d->damage          = j["damage"];
	d->primaryModule   = j["primary_module"];
	d->projectileSpeed = j.value("projectile_speed", 0.0f);
	return d;
}

//-----------------------------------------------------------------------------
// HitscanModule
//-----------------------------------------------------------------------------
void HitscanModule::Execute(Entity& entity) {
	auto camera = CameraManager::GetActiveCamera();
	Vec3 eye    = camera->GetViewMat().Inverse().GetTranslate();
	Vec3 fwd    = camera->GetViewMat().Inverse().GetForward();

	auto* collider = entity.GetComponent<BoxColliderComponent>();

	auto result = collider->RayCast(
		eye,
		fwd,
		Math::HtoM(65535.0f) // 最大距離を65535HUに設定
	);

	if (!result.empty()) {
		auto hit = *std::ranges::min_element(result,
		                                     [](const HitResult& a,
		                                        const HitResult& b) {
			                                     return a.dist < b.dist;
		                                     });

		if (!hit.hitEntity) {
			// ヒットしたエンティティがない場合は何もしない
			bHit_ = false;
			return;
		}

		if (hit.isHit) {
			Debug::DrawRay(eye, fwd * hit.dist, Vec4::red);
			Debug::DrawBox(hit.hitPos, Quaternion::identity,
			               Vec3(0.1f, 0.1f, 0.1f), Vec4::green);
			Debug::DrawAxis(hit.hitPos, Quaternion::identity);
			Debug::DrawRay(hit.hitPos, hit.hitNormal, Vec4::magenta);

			//hit.hitEntity->SetActive(false); // ヒットしたエンティティを非アクティブにする
			hitPosition_ = hit.hitPos;    // ヒット位置を保存
			hitNormal_   = hit.hitNormal; // ヒット面の法線を保存
			bHit_        = true;
		}

		Console::Print("Hitscan Fired!\n");
	} else {
		bHit_ = false; // ヒットしなかった場合はフラグをリセット
	}
}

void HitscanModule::Update([[maybe_unused]] const float& deltaTime) {
}

void HitscanModule::DrawInspectorImGui() {
}


namespace {
	std::unique_ptr<IWeaponModule> CreateModule(const WeaponData& d) {
		if (d.primaryModule == "hitscan") {
			return std::make_unique<HitscanModule>(d);
		}
		// ここに他のモジュールの作成処理を追加
		Console::Print("Unknown weapon module: " + d.primaryModule,
		               Vec4::red, Channel::Game);
		return nullptr;
	}
}

//-----------------------------------------------------------------------------
// 近接攻撃モジュール
//-----------------------------------------------------------------------------
// MeleeModule::~MeleeModule() {
// }
//
// void MeleeModule::Execute([[maybe_unused]] Entity& entity) {
// 	auto                  cam     = CameraManager::GetActiveCamera();
// 	[[maybe_unused]] Vec3 forward = cam->GetViewMat().GetForward();
// 	[[maybe_unused]] Vec3 eyePos  = cam->GetViewMat().GetTranslate();
// }
//
// void MeleeModule::Update([[maybe_unused]] const float& deltaTime) {
// }
//
// void MeleeModule::DrawInspectorImGui() {
// }

//-----------------------------------------------------------------------------
// WeaponComponent
//-----------------------------------------------------------------------------
WeaponComponent::~WeaponComponent() {
}

WeaponComponent::WeaponComponent(const std::string& weaponJsonPath):
	mWeaponData(WeaponData::LoadFromJson(weaponJsonPath)) {
	if (!mWeaponData) {
		Console::Print("WeaponComponent: Failed to load weapon data from "
		               "JSON file: " + weaponJsonPath,
		               Vec4::red, Channel::Game);
		return;
	}

	mPrimaryModule = CreateModule(*mWeaponData);
	mCurrentAmmo   = mWeaponData->maxAmmo;
	mCurrentClip   = mWeaponData->clipSize;
}

void WeaponComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void WeaponComponent::Update([[maybe_unused]] float deltaTime) {
	bFiredThisFrame_ = false; // 今フレームで発射したかどうかをリセット

	if (bIsReloading_) {
		reloadTimer_ += deltaTime;
		if (reloadTimer_ >= 0.0f) {
			int need = mWeaponData->clipSize - mCurrentClip;
			int load = (std::min)(need, mCurrentAmmo);
			mCurrentClip += load;
			mCurrentAmmo -= load;
			bIsReloading_ = false;
			Console::Print(
				"Reloaded!\n"
			);
		}
		return;
	}

	timeSinceShot_ += deltaTime;
	if (bTriggerHeld_ && CanFire()) {
		mPrimaryModule->Execute(*GetOwner());
		--mCurrentClip;
		timeSinceShot_   = 0.0f;
		bFiredThisFrame_ = true; // 今フレームで発射した
	}
}

void WeaponComponent::DrawInspectorImGui() {
	ImGui::Text("Weapon: %s", mWeaponData->name.c_str());
	ImGui::Text("Ammo: %d / %d", mCurrentClip, mCurrentAmmo);
	mPrimaryModule->DrawInspectorImGui();
}

void WeaponComponent::PullTrigger() {
	bTriggerHeld_ = true;
	if (CanFire()) {
		mPrimaryModule->Execute(*GetOwner());
		--mCurrentClip;
		timeSinceShot_   = 0.0f;
		bFiredThisFrame_ = true; // 今フレームで発射した
	}
}

void WeaponComponent::ReleaseTrigger() {
	bTriggerHeld_ = false;
}

void WeaponComponent::Reload() {
	if (bIsReloading_ ||
		mCurrentAmmo <= 0 ||
		mCurrentClip == mWeaponData->clipSize) {
		// すでにリロード中、または弾薬がない、またはクリップが満タンの場合は何もしない
		return;
	}
	bIsReloading_ = true;
	reloadTimer_  = mWeaponData->reloadTime;
	Console::Print(
		"Reloading...\n"
	);
}

bool WeaponComponent::CanFire() const {
	return (mCurrentClip > 0) &&
		(timeSinceShot_ >= mWeaponData->fireRate) &&
		!bIsReloading_;
}

Vec3 WeaponComponent::GetHitPosition() const {
	return dynamic_cast<HitscanModule*>(mPrimaryModule.get())->GetHitPosition();
}

Vec3& WeaponComponent::GetHitNormal() {
	return dynamic_cast<HitscanModule*>(mPrimaryModule.get())->GetHitNormal();
}

bool WeaponComponent::HasFiredThisFrame() const {
	return bFiredThisFrame_;
}

Entity* WeaponComponent::GetOwner() const {
	return Component::GetOwner();
}

void WeaponComponent::LoadFromJson(
	[[maybe_unused]] const std::string& jsonPath) {
}
