#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	class IGameModule;

	/// @brief 文字列名から GameModule を生成する静的リンク向けファクトリレジストリです。
	class GameModuleRegistry {
	public:
		/// @brief GameModule 生成関数の型です。
		using CreateFunction = std::unique_ptr<IGameModule> (*)();

		/// @brief モジュール名と生成関数を登録します。
		[[nodiscard]] bool RegisterFactory(
			std::string_view moduleName,
			CreateFunction   createFunction
		);

		/// @brief 指定モジュール名から GameModule を生成します。
		[[nodiscard]] std::unique_ptr<IGameModule> Create(
			std::string_view moduleName
		) const;

		/// @brief 指定モジュール名が登録済みかどうかを返します。
		[[nodiscard]] bool Contains(std::string_view moduleName) const;

		/// @brief 登録済みモジュール名の一覧を返します。
		[[nodiscard]] std::vector<std::string> ListRegisteredNames() const;

	private:
		[[nodiscard]] static std::string NormalizeModuleName(
			std::string_view moduleName
		);

		std::unordered_map<std::string, CreateFunction> mFactories    = {};
		std::unordered_map<std::string, std::string>    mDisplayNames = {};
	};
}
