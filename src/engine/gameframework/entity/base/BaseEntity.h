#pragma once
#include <memory>
#include <string>
#include <vector>

#include <engine/gameframework/component/base/BaseComponent.h>

namespace Unnamed {
	// TODO: 旧Entityを取り除いたらEntityに名前変更

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
		/// @brief エンティティが登録されたときに呼び出されます。
		virtual void OnRegister() = 0;
		/// @brief エンティティが登録された後に呼び出されます。
		virtual void PostRegister() = 0;

		/// @brief 物理演算の前に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void PrePhysicsTick(float deltaTime) = 0;
		/// @brief 毎フレーム呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void Tick(float deltaTime) = 0;
		/// @brief 物理演算の後に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void PostPhysicsTick(float deltaTime) = 0;

		/// @brief レンダリングの前に呼び出されます。
		virtual void OnPreRender() const = 0;
		/// @brief レンダリング時に呼び出されます。
		virtual void OnRender() const = 0;
		/// @brief レンダリングの後に呼び出されます。
		virtual void OnPostRender() const = 0;

		/// @brief エディターのティック時に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void OnEditorTick(float deltaTime);
		/// @brief エディターのレンダリング時に呼び出されます。
		virtual void OnEditorRender() const;

		/// @brief エンティティが破棄されるときに呼び出されます。
		virtual void OnDestroy() = 0;

		//---------------------------------------------------------------------
		// 関数
		//---------------------------------------------------------------------
		/// @brief コンポーネントを追加します。
		/// @tparam ComponentType 追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 追加されたコンポーネントのポインタ
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

		/// @brief 指定した型のコンポーネントを取得します。
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @return 取得したコンポーネントのポインタ。存在しない場合は nullptr。
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

		/// @brief 指定した型のコンポーネントを取得するか、存在しない場合は新たに追加します。
		/// @tparam ComponentType 取得または追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 取得または追加されたコンポーネントのポインタ
		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* GetOrAddComponent(Args&&... args) {
			if (auto* component = GetComponent<ComponentType>()) {
				return component;
			}
			return AddComponent<ComponentType>(std::forward<Args>(args)...);
		}

		/// @brief コンポーネントを削除します。
		/// @param component 削除するコンポーネントのポインタ
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

		/// @brief 所有しているコンポーネントのリストを取得します。
		/// @return コンポーネントのリスト
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
