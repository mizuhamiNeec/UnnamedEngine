#include <fstream>

#include "IniParser.h"

#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "IniParser";

	/// @brief INIファイルをパースする
	/// @param filePath INIファイルのパス
	/// @return セクション名とキー・値のペアのマップ
	std::unordered_map<std::string, std::unordered_map<
		                   std::string, std::string>> IniParser::ParseIniFile(
		const std::string& filePath
	) {
		std::unordered_map<std::string, std::unordered_map<
			                   std::string, std::string>> iniData;
		std::ifstream                                     inputFile(
			Path::FromUtf8(filePath)
		);
		if (!inputFile.is_open()) {
			Msg(
				kChannel, "ファイルを開けませんでした: {}", filePath
			);
			return iniData;
		}

		std::string line;
		std::string currentSection = "Default";
		while (std::getline(inputFile, line)) {
			// コメント、空行は無視
			line = StrUtil::TrimSpaces(line);
			if (line.empty() || line[0] == ';') {
				continue;
			}

			// セクションの判定
			if (line.front() == '[' && line.back() == ']') {
				currentSection = line.substr(1, line.size() - 2);
				continue;
			}

			// キーと値の分割
			size_t equalsPos = line.find('=');
			if (equalsPos != std::string::npos) {
				std::string key =
					StrUtil::TrimSpaces(line.substr(0, equalsPos));
				std::string value = StrUtil::TrimSpaces(
					line.substr(equalsPos + 1)
				);
				iniData[currentSection][key] = value;
			}
		}

		return iniData;
	}
}
