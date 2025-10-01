#include <pch.h>

#include <engine/gameframework/entity/UEntity/UEntity.h>

namespace Unnamed {
	UEntity::~UEntity() {
		UEntity::OnDestroy();
	}

	void UEntity::OnRegister() {
		DevMsg(
			"Entity",
			"UEntity: {} の登録を開始しました。",
			GetName()
		);
	}

	void UEntity::PostRegister() {
		DevMsg(
			"Entity",
			"UEntity: {} の登録が完了しました。",
			GetName()
		);
	}

	void UEntity::PrePhysicsTick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->PrePhysicsTick(deltaTime);
			}
		}
	}

	void UEntity::Tick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnTick(deltaTime);
			}
		}
	}

	void UEntity::PostPhysicsTick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->PostPhysicsTick(deltaTime);
			}
		}
	}

	void UEntity::OnPreRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnPreRender();
			}
		}
	}

	void UEntity::OnRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnRender();
			}
		}
	}

	void UEntity::OnPostRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnPostRender();
			}
		}
	}

	void UEntity::OnDestroy() {
		for (const auto& component : mComponents) {
			RemoveComponent(component.get());
		}
	}
}
