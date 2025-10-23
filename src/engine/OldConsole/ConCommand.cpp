#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/ConVarManager.h>

#include <sstream>


/// @brief コンソールコマンドを初期化します
void ConCommand::Init() {
	RegisterCommand(
		"toggle",
		[](const std::vector<std::string>& args) {
			ConVarManager::ToggleConVar(args[0]);
		},
		"Toggles a convar on or off, or cycles through a set of values."
	);
}

/// @brief コンソールコマンドを登録します
/// @param name コマンド名
/// @param callback コマンドコールバック関数
/// @param help コマンドのヘルプテキスト
void ConCommand::RegisterCommand(const std::string&     name,
                                 const CommandCallback& callback,
                                 const std::string&     help) {
	commands_[name] = {callback, help};
}

/// @brief コンソールコマンドを実行します
/// @param command コマンド文字列
/// @return コマンドが見つかり実行された場合はtrue、そうでなければfalse
bool ConCommand::ExecuteCommand(const std::string& command) {
	auto tokens = TokenizeCommand(command);
	if (tokens.empty()) {
		return false;
	}

	const auto& cmdName = tokens[0];
	auto        it      = commands_.find(cmdName);
	if (it != commands_.end()) {
		const auto&       callback = it->second.first;
		const std::vector args(tokens.begin() + 1, tokens.end());
		callback(args);
		return true;
	}

	return false;
}

/// @brief 登録されている全てのコマンドを取得します
/// @return コマンド名とコールバック関数、ヘルプテキストのペアのマップ
std::unordered_map<std::string, std::pair<CommandCallback, std::string>>
ConCommand::GetCommands() {
	return commands_;
}

/// @brief ヘルプを表示します
void ConCommand::Help() {
	for (const auto& [commandName, commandData] : commands_) {
		Console::Print(" - " + commandName + " : " + commandData.second + "\n",
		               kConFgColorDark, Channel::None);
	}
}

/// @brief コマンドをトークン化します
/// @param command コマンド文字列
/// @return トークンのベクター
std::vector<std::string>
ConCommand::TokenizeCommand(const std::string& command) {
	std::istringstream       stream(command);
	std::vector<std::string> tokens;
	std::string              token;
	while (stream >> token) {
		tokens.emplace_back(token);
	}
	return tokens;
}

std::unordered_map<std::string, std::pair<
	                   ConCommand::CommandCallback, std::string>>
ConCommand::commands_;
