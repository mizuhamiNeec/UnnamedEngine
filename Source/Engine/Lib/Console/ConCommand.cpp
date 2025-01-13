#include "ConCommand.h"

#include "Console.h"

void ConCommand::RegisterCommand(const std::string& name, const CommandCallback& callback, const std::string& help) {
	commands_[name] = { callback, help };
}

bool ConCommand::ExecuteCommand(const std::string& command) {
	auto tokens = TokenizeCommand(command);
	if (tokens.empty()) {
		return false;
	}

	const auto& cmdName = tokens[0];
	auto it = commands_.find(cmdName);
	if (it != commands_.end()) {
		const auto& callback = it->second.first;
		const std::vector args(tokens.begin() + 1, tokens.end());
		callback(args);
		return true;
	}

	return false;
}

std::unordered_map<std::string, std::pair<CommandCallback, std::string>> ConCommand::GetCommands() {
	return commands_;
}

void ConCommand::Help() {
	for (const auto& [commandName, commandData] : commands_) {
		Console::Print(" - " + commandName + " : " + commandData.second + "\n", kConsoleColorNormal, Channel::None);
	}
}

std::vector<std::string> ConCommand::TokenizeCommand(const std::string& command) {
	std::istringstream stream(command);
	std::vector<std::string> tokens;
	std::string token;
	while (stream >> token) {
		tokens.push_back(token);
	}
	return tokens;
}

std::unordered_map<std::string, std::pair<ConCommand::CommandCallback, std::string>> ConCommand::commands_;
