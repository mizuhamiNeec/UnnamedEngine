#pragma once
#include <memory>
#include <string>
#include <vector>

#include <engine/public/gameframework/component/base/BaseComponent.h>

namespace Unnamed {
	// TODO: 従来版を取り除いたらEntityに名前変更

	/// @class BaseEntity
	/// @brief エンティティはゲームの基本オブジェクトです。
	/// ゲーム内に登場したり、ロジックを持つオブジェクトを表します。
	/// "ECSとは一切関係ありません"
	class BaseEntity {
	public:
		explicit BaseEntity(std::string name);
		virtual  ~BaseEntity();

		//---------------------------------------------------------------------
		// 純粋仮想関数
		//---------------------------------------------------------------------
		virtual void OnRegister() = 0;
		virtual void PostRegister() = 0;

		virtual void PrePhysicsTick(float deltaTime) = 0;
		virtual void Tick(float deltaTime) = 0;
		virtual void PostPhysicsTick(float deltaTime) = 0;

		virtual void OnPreRender() const = 0;
		virtual void OnRender() const = 0;
		virtual void OnPostRender() const = 0;

		virtual void OnEditorTick(float deltaTime);
		virtual void OnEditorRender() const;

		virtual void OnDestroy() = 0;

		//---------------------------------------------------------------------
		// 関数
		//---------------------------------------------------------------------
		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* AddComponent(Args&&... args) {
			static_assert(
				std::is_base_of_v<BaseComponent, ComponentType>,
				"T は BaseComponent の派生クラスでなければなりません。"
			);
			auto component = std::make_unique<ComponentType>(
				std::forward<Args>(args)...
			);
			component->SetOwner(this); // 所有者を設定
			component->OnAttached();   // 取り付け時の処理を呼び出す
			mComponents.emplace_back(std::move(component));
			return static_cast<ComponentType*>(mComponents.back().get());
		}

		template <typename ComponentType>
		[[nodiscard]] ComponentType* GetComponent() {
			static_assert(
				std::is_base_of_v<BaseComponent, ComponentType>,
				"T は BaseComponent の派生クラスでなければなりません。"
			);
			for (const auto& component : mComponents) {
				if (
					auto* casted = dynamic_cast<ComponentType*>(component.get())
				) {
					return casted;
				}
			}
			return nullptr;
		}

		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* GetOrAddComponent(Args&&... args) {
			if (auto* component = GetComponent<ComponentType>()) {
				return component;
			}
			return AddComponent<ComponentType>(std::forward<Args>(args)...);
		}

		void RemoveComponent(BaseComponent* component) {
			if (!component) return;

			const auto it = std::ranges::find_if(
				mComponents,
				[component](
				const std::unique_ptr<BaseComponent>& comp) {
					return comp.get() == component;
				}
			);

			if (it != mComponents.end()) {
				(*it)->OnDetached();
				mComponents.erase(it);
			}
		}

		[[nodiscard]] const std::vector<std::unique_ptr<BaseComponent>>&
		GetComponents() const {
			return mComponents;
		}

		[[nodiscard]] std::string_view GetName() const;
		void                           SetName(std::string& name);

		[[nodiscard]] bool IsEditorOnly() const noexcept;

		[[nodiscard]] uint64_t GetId() const noexcept;

	protected:
		std::string mName;                 // 名前
		bool        mIsEditorOnly = false; // エディター専用か?
		uint64_t    mId           = 0;     // GUID

		// 所有しているコンポーネント
		std::vector<std::unique_ptr<BaseComponent>> mComponents;
	};
}
