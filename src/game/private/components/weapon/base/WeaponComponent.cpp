

#include "game/public/components/weapon/base/WeaponComponent.h"

#include <fstream>
#include <json.hpp>

#include "engine/public/Camera/CameraManager.h"
#include "engine/public/Components/Camera/CameraComponent.h"
#include "engine/public/Components/ColliderComponent/BoxColliderComponent.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/OldConsole/Console.h"
#include "engine/public/Physics/PhysicsEngine.h"

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
	const auto camera = CameraManager::GetActiveCamera();
	const Vec3 eye    = camera->GetViewMat().Inverse().GetTranslate();
	const Vec3 fwd    = camera->GetViewMat().Inverse().GetForward();

	const auto* collider = entity.GetComponent<BoxColliderComponent>();

	auto result = collider->RayCast(
		eye,
		fwd,
		Math::HtoM(65535.0f) // 最大距離を65535HUに設定
	);

	if (!result.empty()) {
		const auto hit = *std::ranges::min_element(result,
		                                     [](const HitResult& a,
		                                        const HitResult& b) {
			                                     return a.dist < b.dist;
		                                     });

		if (!hit.hitEntity) {
			// ヒットしたエンティティがない場合は何もしない
			mIsHit = false;
			return;
		}

		if (hit.isHit) {
			Debug::DrawRay(eye, fwd * hit.dist, Vec4::red);
			Debug::DrawBox(hit.hitPos, Quaternion::identity,
			               Vec3(0.1f, 0.1f, 0.1f), Vec4::green);
			Debug::DrawAxis(hit.hitPos, Quaternion::identity);
			Debug::DrawRay(hit.hitPos, hit.hitNormal, Vec4::magenta);

			//hit.hitEntity->SetActive(false); // ヒットしたエンティティを非アクティブにする
			mHitPosition = hit.hitPos;    // ヒット位置を保存
			mHitNormal   = hit.hitNormal; // ヒット面の法線を保存
			mIsHit        = true;
		}

		Console::Print("Hitscan Fired!\n");
	} else {
		mIsHit = false; // ヒットしなかった場合はフラグをリセット
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
	mFiredThisFrame = false; // 今フレームで発射したかどうかをリセット

	if (mIsReloading) {
		mReloadTimer += deltaTime;
		if (mReloadTimer >= 0.0f) {
			int need = mWeaponData->clipSize - mCurrentClip;
			int load = (std::min)(need, mCurrentAmmo);
			mCurrentClip += load;
			mCurrentAmmo -= load;
			mIsReloading = false;
			Console::Print(
				"Reloaded!\n"
			);
		}
		return;
	}

	mTimeSinceShot += deltaTime;
	if (mTriggerHeld && CanFire()) {
		mPrimaryModule->Execute(*GetOwner());
		--mCurrentClip;
		mTimeSinceShot   = 0.0f;
		mFiredThisFrame = true; // 今フレームで発射した
	}
}

void WeaponComponent::DrawInspectorImGui() {
	ImGui::Text("Weapon: %s", mWeaponData->name.c_str());
	ImGui::Text("Ammo: %d / %d", mCurrentClip, mCurrentAmmo);
	mPrimaryModule->DrawInspectorImGui();
}

void WeaponComponent::PullTrigger() {
	mTriggerHeld = true;
	if (CanFire()) {
		mPrimaryModule->Execute(*GetOwner());
		--mCurrentClip;
		mTimeSinceShot   = 0.0f;
		mFiredThisFrame = true; // 今フレームで発射した
	}
}

void WeaponComponent::ReleaseTrigger() {
	mTriggerHeld = false;
}

void WeaponComponent::Reload() {
	if (mIsReloading ||
		mCurrentAmmo <= 0 ||
		mCurrentClip == mWeaponData->clipSize) {
		// すでにリロード中、または弾薬がない、またはクリップが満タンの場合は何もしない
		return;
	}
	mIsReloading = true;
	mReloadTimer  = mWeaponData->reloadTime;
	Console::Print(
		"Reloading...\n"
	);
}

bool WeaponComponent::CanFire() const {
	return (mCurrentClip > 0) &&
		(mTimeSinceShot >= mWeaponData->fireRate) &&
		!mIsReloading;
}

Vec3 WeaponComponent::GetHitPosition() const {
	return dynamic_cast<HitscanModule*>(mPrimaryModule.get())->GetHitPosition();
}

Vec3& WeaponComponent::GetHitNormal() const {
	return dynamic_cast<HitscanModule*>(mPrimaryModule.get())->GetHitNormal();
}

bool WeaponComponent::HasFiredThisFrame() const {
	return mFiredThisFrame;
}

Entity* WeaponComponent::GetOwner() const {
	return Component::GetOwner();
}

void WeaponComponent::LoadFromJson(
	[[maybe_unused]] const std::string& jsonPath) {
}
