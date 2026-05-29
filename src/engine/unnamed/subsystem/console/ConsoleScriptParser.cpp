#include "ConsoleScriptParser.h"

#include <fstream>

#include <core/string/StrUtil.h>
#include <core/path/PathUtil.h>

#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "ConScrP";

	/// @brief コンストラクタ
	/// @param path スクリプトファイルのパス
	ConsoleScriptParser::ConsoleScriptParser(const std::string_view& path) {
		const std::filesystem::path nativePath = Path::FromUtf8(path);
		std::ifstream               inputFile(nativePath);

		// 存在しない場合は作る
		if (!inputFile) {
			Msg(
				kChannel, "Script file not found. Creating a new one: {}",
				std::string(path)
			);
			std::ofstream outputFile(nativePath);
			if (!outputFile) {
				Error(
					kChannel, "Failed to create script file: {}",
					std::string(path)
				);
				throw std::runtime_error("Failed to create script file");
			}
			outputFile.close();
			inputFile.open(nativePath);
		}

		if (!inputFile.is_open()) {
			Error(
				kChannel, "Failed to open script file: {}", std::string(path)
			);
			throw std::runtime_error("Failed to open script file");
		}

		Msg(kChannel, "Executing script file: {}", std::string(path));

		std::string line;
		while (std::getline(inputFile, line)) {
			line = StrUtil::TrimSpaces(line);
			// 空行またはコメント行をスキップ
			if (
				line.empty() ||
				line[0] == ';' ||
				line[0] == '/' && line.size() > 1 && line[1] == '/'
			) {
				continue;
			}

			ServiceLocator::Get<ConsoleSystem>()->ExecuteCommand(line);
		}
	}
}
