#include <pch.h>

#include <engine/gameframework/entity/UEntity/UEntity.h>

namespace Unnamed {
	/// @brief デストラクタ
	UEntity::~UEntity() {
		UEntity::OnDestroy();
	}

	/// @brief 登録開始時の処理
	void UEntity::OnRegister() {
		DevMsg(
			"Entity",
			"UEntity: {} の登録を開始しました。",
			GetName()
		);
	}

	/// @brief 登録完了時の処理
	void UEntity::PostRegister() {
		DevMsg(
			"Entity",
			"UEntity: {} の登録が完了しました。",
			GetName()
		);
	}

	/// @brief 物理演算前の更新処理
	/// @param deltaTime 前フレームからの経過時間（秒）
	void UEntity::PrePhysicsTick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->PrePhysicsTick(deltaTime);
			}
		}
	}

	/// @brief 通常の更新処理
	/// @param deltaTime 前フレームからの経過時間（秒）
	void UEntity::Tick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnTick(deltaTime);
			}
		}
	}

	/// @brief 物理演算後の更新処理
	/// @param deltaTime 前フレームからの経過時間（秒）
	void UEntity::PostPhysicsTick(const float deltaTime) {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->PostPhysicsTick(deltaTime);
			}
		}
	}

	/// @brief 描画前の処理
	void UEntity::OnPreRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnPreRender();
			}
		}
	}

	/// @brief 描画処理
	void UEntity::OnRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnRender();
			}
		}
	}

	///	@brief 描画後の処理
	void UEntity::OnPostRender() const {
		for (const auto& component : mComponents) {
			if (component->IsActive()) {
				component->OnPostRender();
			}
		}
	}

	/// @brief 破棄時の処理
	void UEntity::OnDestroy() {
		for (const auto& component : mComponents) {
			RemoveComponent(component.get());
		}
	}
}
