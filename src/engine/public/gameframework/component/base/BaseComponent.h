#pragma once
#include <string_view>

namespace Unnamed {
	/// @class BaseComponent
	/// @brief コンポーネントはエンティティに取り付けられるオブジェクトです。
	/// 取り付けられたエンティティにどの用に振る舞わせるかを定義します。
	class BaseComponent {
	public:
		explicit BaseComponent(std::string name);
		virtual  ~BaseComponent() = default;

		//---------------------------------------------------------------------
		// 純粋仮想関数
		//---------------------------------------------------------------------
		virtual void OnAttached() = 0;
		virtual void OnDetached() = 0;

		virtual void PrePhysicsTick(float deltaTime) = 0;
		virtual void OnTick(float deltaTime) = 0;
		virtual void PostPhysicsTick(float deltaTime) = 0;

		virtual void OnPreRender() const = 0;
		virtual void OnRender() const = 0;
		virtual void OnPostRender() const = 0;

		virtual void OnEditorTick(float deltaTime);
		virtual void OnEditorRender() const;

		[[nodiscard]] virtual std::string_view GetComponentName() const = 0;

		//---------------------------------------------------------------------
		// 関数
		//---------------------------------------------------------------------
		[[nodiscard]] std::string_view GetName();

		[[nodiscard]] class BaseEntity* GetOwner() const {
			return mOwner; // 所有者を取得
		}

		void SetOwner(BaseEntity* owner) {
			mOwner = owner; // 所有者を設定
		}

		[[nodiscard]] bool IsActive() const {
			return mIsActive; // アクティブかどうかを取得
		}

	protected:
		class BaseEntity* mOwner = nullptr; // 所有しているエンティティ

		std::string mName;            // 名前
		bool        mIsActive = true; // アクティブか?
		uint64_t    mId       = 0;    // GUID
	};
}
