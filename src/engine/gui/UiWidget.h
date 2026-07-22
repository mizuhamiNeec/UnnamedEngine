#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "Rect.h"
#include "UiDrawCommand.h"
#include "components/UiComponent.h"

namespace Unnamed::Gui {
	/// @brief ウィジェットのダーティフラグ
	enum class DIRTY_FLAGS : uint32_t {
		NONE   = 0,
		LAYOUT = 1 << 0,
		STYLE  = 1 << 1,
		DRAW   = 1 << 2,
		ALL    = LAYOUT | STYLE | DRAW,
	};

	/// @brief ビット演算子のオーバーロード
	DIRTY_FLAGS operator|(DIRTY_FLAGS a, DIRTY_FLAGS b);
	/// @brief 複合代入ビット演算子のオーバーロード
	DIRTY_FLAGS operator|=(DIRTY_FLAGS& a, DIRTY_FLAGS b);
	/// @brief フラグが立っているかどうかを判定します。
	bool HasFlag(DIRTY_FLAGS flags, DIRTY_FLAGS test);
	/// @brief DIRTY_FLAGSを文字列に変換します。
	const char* ToString(DIRTY_FLAGS e);

	enum class UiSizePolicyAxis {
		FIXED,  // ローカルサイズをそのまま使用
		EXPAND, // 親にねだる
	};

	struct UiSizePolicy {
		UiSizePolicyAxis horizontal = UiSizePolicyAxis::FIXED;
		UiSizePolicyAxis vertical   = UiSizePolicyAxis::FIXED;
	};

	struct UiSizeConstraints {
		float minWidth  = 0.0f;
		float minHeight = 0.0f;
		float maxWidth  = std::numeric_limits<float>::max();
		float maxHeight = std::numeric_limits<float>::max();
	};

	/// @brief UIウィジェットの基本クラス
	class UiWidget {
	public:
		UiWidget();
		virtual ~UiWidget();

		void AddComponent(std::unique_ptr<UiComponent> component);

		template <typename ComponentType, typename... Args>
		ComponentType* AddComponent(Args&&... args) {
			static_assert(
				std::is_base_of_v<UiComponent, ComponentType>,
				"ComponentType must derive from UiComponent"
			);
			auto component = std::make_unique<ComponentType>(
				std::forward<Args>(args)...
			);
			ComponentType* raw = component.get();
			AddComponent(std::move(component));
			return raw;
		}

		template <typename ComponentType>
		[[nodiscard]] ComponentType* GetComponent() {
			static_assert(
				std::is_base_of_v<UiComponent, ComponentType>,
				"ComponentType must derive from UiComponent"
			);
			for (auto& component : mComponents) {
				if (auto* casted = dynamic_cast<ComponentType*>(component.
					get())) {
					return casted;
				}
			}
			return nullptr;
		}

		template <typename ComponentType>
		[[nodiscard]] const ComponentType* GetComponent() const {
			static_assert(
				std::is_base_of_v<UiComponent, ComponentType>,
				"ComponentType must derive from UiComponent"
			);
			for (const auto& component : mComponents) {
				if (auto* casted =
					dynamic_cast<const ComponentType*>(component.get())) {
					return casted;
				}
			}
			return nullptr;
		}

		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* GetOrAddComponent(Args&&... args) {
			if (auto* existing = GetComponent<ComponentType>()) {
				return existing;
			}
			return AddComponent<ComponentType>(std::forward<Args>(args)...);
		}

		[[nodiscard]] UiComponent* GetComponentByTypeName(
			std::string_view typeName
		) const;
		[[nodiscard]] const std::vector<std::unique_ptr<UiComponent>>&
		GetComponents() const;
		[[nodiscard]] std::vector<std::unique_ptr<UiComponent>>&
		GetComponents();
		bool RemoveComponentAt(size_t index);
		bool MoveComponent(size_t fromIndex, size_t toIndex);
		[[nodiscard]] static std::unique_ptr<UiComponent>
		CreateComponentByTypeName(
			std::string_view typeName
		);
		[[nodiscard]] static std::vector<std::string_view>
		GetRegisteredComponentTypeNames();

		/// @brief 子を追加します。
		/// @param child 追加する子ウィジェット
		void AddChild(std::unique_ptr<UiWidget> child);
		/// @brief 子を削除します。
		/// @param child 削除する子ウィジェット
		void RemoveChild(const UiWidget* child);
		/// @brief 子の所有権を取り出します。
		/// @param child 取り出す子ウィジェット
		/// @return 取り出された子ウィジェット（見つからない場合はnullptr）
		std::unique_ptr<UiWidget> TakeChild(const UiWidget* child);

		void AddChildReference(UiWidget* child);
		void RemoveChildReference(const UiWidget* child);
		const std::vector<UiWidget*>& GetReferenceChildren() const;

		/// @brief 子ウィジェットの一覧を取得します。
		/// @return 子ウィジェットの一覧への参照
		[[nodiscard]]
		const std::vector<std::unique_ptr<UiWidget>>& GetChildren() const;

		/// @brief 親ウィジェットを取得します。
		/// @return 親ウィジェットへのポインタ（親がいない場合はnullptr）
		[[nodiscard]] UiWidget* GetParent() const;

		/// @brief ウィジェットの名前を設定します。
		/// @param name 新しいウィジェットの名前
		void SetName(const std::string_view& name);
		/// @brief ウィジェットの名前を取得します。
		/// @return ウィジェットの名前
		[[nodiscard]] std::string_view GetName() const;

		/// @brief ローカル矩形を設定します。
		/// @param rect 新しいローカル矩形
		void SetLocalRect(const Rect& rect);
		/// @brief ローカル矩形を取得します。
		/// @return ローカル矩形への参照
		[[nodiscard]]
		const Rect& GetLocalRect() const;

		/// @brief グローバル矩形を取得します。
		/// @return グローバル矩形への参照
		[[nodiscard]]
		const Rect& GetGlobalRect() const;

		/// @brief アンカーを設定します。
		/// @param anchors 新しいアンカー設定
		void SetAnchors(const Anchors& anchors);
		/// @brief アンカー設定を取得します。
		/// @return アンカー設定への参照
		[[nodiscard]] const Anchors& GetAnchors() const;

		/// @brief マージンを設定します。
		/// @param margins 新しいマージン設定
		void SetMargins(const Margins& margins);
		/// @brief マージン設定を取得します。
		/// @return マージン設定への参照
		[[nodiscard]] const Margins& GetMargins() const;

		/// @brief ピボットを設定します。
		/// @param pivot 新しいピボット設定
		void SetPivot(const Pivot& pivot);
		/// @brief ピボット設定を取得します。
		/// @return ピボット設定への参照
		[[nodiscard]] const Pivot& GetPivot() const;

		/// @brief ウィジェットの表示・非表示を設定します。
		/// @param visible trueで表示、falseで非表示
		void SetVisible(bool visible);
		/// @brief ウィジェットが表示されているかどうかを取得します。
		/// @return 表示されている場合はtrue、非表示の場合はfalse
		[[nodiscard]] bool IsVisible() const;

		/// @brief ウィジェットの有効・無効を設定します。
		/// @param enabled trueで有効、falseで無効
		void SetEnabled(bool enabled);
		/// @brief ウィジェットが有効かどうかを取得します。
		/// @return 有効な場合はtrue、無効な場合はfalse
		[[nodiscard]] bool IsEnabled() const;

		/// @brief ダーティフラグを設定します。
		/// @param flags 設定するダーティフラグ
		void MarkDirty(DIRTY_FLAGS flags);
		/// @brief 現在のダーティフラグを取得します。
		/// @return 現在のダーティフラグ
		[[nodiscard]] DIRTY_FLAGS GetDirtyFlags() const;
		/// @brief 指定したダーティフラグをクリアします。
		/// @param flags クリアするダーティフラグ
		void ClearDirtyFlags(DIRTY_FLAGS flags);

		void SetSizePolicy(
			UiSizePolicyAxis horizontal, UiSizePolicyAxis vertical
		);
		UiSizePolicy GetSizePolicy() const;

		void SetSizeConstraints(const UiSizeConstraints& constraints);

		[[nodiscard]] const UiSizeConstraints& GetSizeConstraints() const;

		[[nodiscard]] float GetPreferredWidth() const;
		[[nodiscard]] float GetPreferredHeight() const;

		/// @brief ウィジェットの更新処理
		virtual void Tick(float deltaTime);

		/// @brief レイアウトを再帰的に更新します。
		/// @param parentGlobalRect 親ウィジェットのグローバル
		virtual void UpdateLayoutRecursive(const Rect& parentGlobalRect);
		/// @brief 描画コマンドを構築します。
		/// @param out 描画コマンドの出力先ベクター
		virtual void BuildDrawCommands(std::vector<UiDrawCommand>& out) const;

		/// @brief デバッグ用のUI描画
		/// @param w 描画するウィジェット
		static void DebugDrawUi(const UiWidget* w);

		/// @brief 指定した座標にヒットしているウィジェットを取得します。
		/// @param x X座標
		/// @param y Y座標
		/// @return ヒットしているウィジェットへのポインタ（ヒットしていない場合はnullptr）
		[[nodiscard]] virtual UiWidget* HitTest(float x, float y);

		/// @brief マウスがホバーしているかどうかを取得します。
		/// @return ホバーしている場合はtrue、していない場合はfalse
		[[nodiscard]] bool IsHovered() const;
		/// @brief マウスが押されているかどうかを取得します。
		/// @return 押されている場合はtrue、押されていない場合はfalse
		[[nodiscard]] bool IsPressed() const;

		/// @brief マウスがウィジェットに入ったときに呼び出されます。
		virtual void OnMouseEnter();
		/// @brief マウスがウィジェットから出たときに呼び出されます。
		virtual void OnMouseLeave();
		/// @brief マウスボタンが押されたときに呼び出されます。
		virtual void OnMouseDown();
		/// @brief マウスボタンが離されたときに呼び出されます。
		virtual void OnMouseUp();
		/// @brief クリックされたときに呼び出されます。
		virtual void OnClick();

		[[nodiscard]] virtual const char* GetTypeName() const;

		virtual void SaveToJson(JsonWriter& writer) const;
		/// @brief UIウィジェットをJSONから復元します。
		/// @return コンポーネントと子を含めて復元できた場合はtrue。
		[[nodiscard]] virtual bool LoadFromJson(
			const JsonReader& reader, const UiDeserializeContext& context
		);

		[[nodiscard]] static std::unique_ptr<UiWidget> CreateFromJson(
			const JsonReader& reader, const UiDeserializeContext& context
		);

	protected:
		/// @brief レイアウトを更新します。
		/// @param parentGlobalRect 親ウィジェットのグローバル矩形
		virtual void UpdateLayout(const Rect& parentGlobalRect);
		/// @brief ダーティフラグが追加されたときに呼び出されます。
		/// @param flags 追加されたダーティフラグ
		virtual void OnDirtyFlagAdded(DIRTY_FLAGS flags);

		virtual void OnSerialize(JsonWriter& writer) const;
		virtual void OnDeserialize(const JsonReader& reader);

	private:
		UiWidget*                              mParent = nullptr;
		std::vector<std::unique_ptr<UiWidget>> mChildren;

		std::vector<UiWidget*> mReferenceChildren;

		std::string mName;

		Rect mLocalRect;
		Rect mGlobalRect;

		Anchors mAnchors;
		Margins mMargins;
		Pivot   mPivot;

		DIRTY_FLAGS mDirtyFlags = DIRTY_FLAGS::ALL;

		bool mVisible = true;
		bool mEnabled = true;

		bool mHovered = false;
		bool mPressed = false;

		UiSizePolicy      mSizePolicy;
		UiSizeConstraints mSizeConstraints;

		std::vector<std::unique_ptr<UiComponent>> mComponents;
	};
}
