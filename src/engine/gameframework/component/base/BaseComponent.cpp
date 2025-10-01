#include <engine/gameframework/component/base/BaseComponent.h>

namespace Unnamed {
	BaseComponent::BaseComponent() = default;

	/// @brief エンティティに取り付けられた際に呼び出されます。
	void BaseComponent::OnAttached() {
	}

	/// @brief エンティティから取り外された際に呼び出されます。
	void BaseComponent::OnDetached() {
	}

	/// @brief 物理エンジンの更新前に呼び出されます。
	void BaseComponent::PrePhysicsTick(float) {
	}

	/// @brief 毎フレーム呼び出されます。
	void BaseComponent::OnTick(float) {
	}

	/// @brief 物理エンジンの更新後に呼び出されます。
	void BaseComponent::PostPhysicsTick(float) {
	}

	/// @brief 描画前に呼び出されます。
	void BaseComponent::OnPreRender() const {
	}

	/// @brief 描画時に呼び出されます。
	void BaseComponent::OnRender() const {
	}

	/// @brief 描画後に呼び出されます。
	void BaseComponent::OnPostRender() const {
	}

	/// @brief エディタモードで毎フレーム呼び出されます。
	void BaseComponent::OnEditorTick(float) {
	}

	/// @brief エディタモードで描画時に呼び出されます。
	void BaseComponent::OnEditorRender() const {
	}

	/// @brief 所有者を設定します。
	void BaseComponent::SetOwner(BaseEntity* owner) {
		mOwner = owner; // 所有者を設定
	}

	/// @brief コンポーネントがアクティブかどうかを取得します。
	bool BaseComponent::IsActive() const noexcept {
		return mIsActive; // アクティブかどうかを取得
	}

	/// @brief コンポーネントのGUIDを取得します。
	uint64_t BaseComponent::GetId() const noexcept {
		return mId;
	}

	/// @brief 所有者を取得します。
	BaseEntity* BaseComponent::GetOwner() const {
		return mOwner; // 所有者を取得
	}
}
