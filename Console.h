#pragma once
#include <string>
#include <vector>

class Console {
public:
	void Init();
	void Update();

	void Print(std::string message);
	void OutputLog(std::string filepath, std::string log);
	void ToggleConsole();
	void SubmitCommand(const std::string& command);

private:
	std::vector<std::string> history_;

	bool wishScrollToBottom = false;

	bool bShowConsole = false;
};

