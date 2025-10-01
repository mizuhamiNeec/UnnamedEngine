#pragma once
#include <functional>
#include <string>

class ConCommand {
public:
	static void Init();

	using CommandCallback = std::function<void(const std::vector<std::string>&)>;
	static void RegisterCommand(const std::string& name, const CommandCallback& callback, const std::string& help);
	static bool ExecuteCommand(const std::string& command);

	static std::unordered_map<std::string, std::pair<CommandCallback, std::string>> GetCommands();

	static void Help();

private:
	static std::vector<std::string> TokenizeCommand(const std::string& command);

	static std::unordered_map<std::string, std::pair<CommandCallback, std::string>> commands_;
};
