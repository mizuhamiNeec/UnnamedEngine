#pragma once

#include <string_view>
#include <vector>

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;
}

namespace Unnamed::Gui {
	struct UiDrawCommand;
	struct UiDeserializeContext;
	class UiWidget;

	/// @brief UiComponentは、entity上のGUI widget参照と有効状態をcomponent lifecycleへ接続します
	class UiComponent {
	public:
		virtual ~UiComponent() = default;

		[[nodiscard]] virtual std::string_view GetTypeName() const = 0;

		virtual void OnAttached(UiWidget& owner);
		virtual void OnDetached(UiWidget& owner);
		virtual void OnTick(UiWidget& owner, float deltaTime);
		virtual void OnBeforeLayout(UiWidget& owner);
		virtual void OnAfterLayout(UiWidget& owner);
		virtual void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const;
		virtual void OnMouseEnter(UiWidget& owner);
		virtual void OnMouseLeave(UiWidget& owner);
		virtual void OnMouseDown(UiWidget& owner);
		virtual void OnMouseUp(UiWidget& owner);
		virtual void OnClick(UiWidget& owner);

		virtual void Serialize(JsonWriter& writer) const;
		/// @brief UIコンポーネントをJSONから復元します。
		/// @return 復元に成功した場合はtrue。
		[[nodiscard]] virtual bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		);
	};
}
