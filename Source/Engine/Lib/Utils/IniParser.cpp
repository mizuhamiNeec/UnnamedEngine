#include "IniParser.h"

#include <fstream>

#include "SubSystem/Console/Console.h"

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> IniParser::ParseIniFile(
	const std::string& filePath
) {
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> iniData;
	std::ifstream inputFile(filePath);
	if (!inputFile.is_open()) {
		Console::Print("ファイルを開けませんでした: " + filePath + "\n", kConTextColorError);
		return iniData;
	}

	std::string line;
	std::string currentSection = "Default";
	while (std::getline(inputFile, line)) {
		// コメント、空行は無視
		line = Trim(line);
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
			std::string key = Trim(line.substr(0, equalsPos));
			std::string value = Trim(line.substr(equalsPos + 1));
			iniData[currentSection][key] = value;
		}
	}

	return iniData;
}

std::string IniParser::Trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	size_t last = str.find_last_not_of(" \t\r\n");
	if (first == std::string::npos || last == std::string::npos) {
		return "";
	}
	return str.substr(first, last - first + 1);
}
