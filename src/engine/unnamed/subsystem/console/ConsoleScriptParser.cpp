#include "ConsoleScriptParser.h"

#include <fstream>

#include <core/string/StrUtil.h>
#include <core/filesystem/Path.h>

#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "ConScrP";

	// TODO: 仮想パスに対応していないので要修正
	
	void ConsoleScriptParser::ParseAndExecute(const Path& path) {
		const auto& native = path.Native();

		// ファイルを開く
		std::ifstream inputFile(native);

		// 存在しない場合は作る
		if (!inputFile) {
			Msg(
				kChannel, "Script file not found. Creating a new one: {}",
				std::string(path)
			);
			std::ofstream outputFile(native);
			if (!outputFile) {
				Error(
					kChannel, "Failed to create script file: {}",
					std::string(path)
				);
				throw std::runtime_error("Failed to create script file");
			}
			outputFile.close();
			inputFile.open(native);
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
