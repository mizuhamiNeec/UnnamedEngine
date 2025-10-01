#pragma once
#include <string_view>

class JsonWriter;
class JsonReader;

namespace Unnamed {
	/// @class BaseComponent
	/// @brief コンポーネントはエンティティに取り付けられるオブジェクトです。
	/// 取り付けられたエンティティにどの用に振る舞わせるかを定義します。
	class BaseComponent {
	public:
		explicit BaseComponent();
		virtual  ~BaseComponent() = default;

		//---------------------------------------------------------------------
		// 純粋仮想関数
		//---------------------------------------------------------------------
		virtual void OnAttached();
		virtual void OnDetached();

		virtual void PrePhysicsTick(float deltaTime);
		virtual void OnTick(float deltaTime);
		virtual void PostPhysicsTick(float deltaTime);

		virtual void OnPreRender() const;
		virtual void OnRender() const;
		virtual void OnPostRender() const;

		virtual void OnEditorTick(float deltaTime);
		virtual void OnEditorRender() const;

		// コンポーネントの値を書き込む際に使用されます。 
		virtual void Serialize(JsonWriter& writer) const = 0;

		// コンポーネントの値を読み込む際に使用されます。
		virtual void Deserialize(const JsonReader& reader) = 0;

		// エディタでの表示やコンソールのログに使用されます。
		[[nodiscard]] virtual std::string_view GetComponentName() const = 0;

		//---------------------------------------------------------------------
		// 関数
		//---------------------------------------------------------------------		
		[[nodiscard]] class BaseEntity* GetOwner() const;

		void SetOwner(BaseEntity* owner);

		[[nodiscard]] bool IsActive() const noexcept;

		[[nodiscard]] uint64_t GetId() const noexcept;

	protected:
		BaseEntity* mOwner = nullptr; // 所有しているエンティティ

		bool     mIsActive = true; // アクティブか?
		uint64_t mId       = 0;    // GUID
	};
}
