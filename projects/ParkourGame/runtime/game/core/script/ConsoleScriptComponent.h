#pragma once
#include <string>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief Attach/Detach時にコンソールコマンドを実行するコンポーネントです。
	/// @details 各配列要素は1行1コマンドとして扱われ、配列順で実行されます。
	class ConsoleScriptComponent : public BaseComponent {
	public:
		/// @brief コンポーネントAttach時に登録済みコマンドを実行します。
		void OnAttached() override;

		/// @brief コンポーネントDetach時に登録済みコマンドを実行します。
		void OnDetached() override;

		/// @brief 永続的に識別可能なコンポーネント名を返します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief エディタ表示用コンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief コンポーネントアイコンIDを返します。
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef	_DEBUG
		/// @brief Debugビルド時にInspectorからコマンド配列を編集します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSONからAttach/Detachコマンド配列を読み込みます。
		void Deserialize(const JsonReader& reader) override;

		/// @brief Attach/Detachコマンド配列をJSONへ保存します。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief 指定フェーズのコマンド配列を順序どおりに実行します。
		void ExecutePhaseCommands(
			const std::vector<std::string>& commands,
			const char*                     phaseName
		) const;

		std::vector<std::string> mOnAttachCommands;
		std::vector<std::string> mOnDetachCommands;
	};
}
