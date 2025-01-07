#pragma once
#include <Windows.h>
#include <string>
#include <unordered_map>

#include "../Lib/Math/Vector/Vec2.h"

class InputSystem {
public:
	static void Init();
	static void Update();

	static void ProcessInput(LPARAM lParam);

	static Vec2 GetMouseDelta();

	static bool IsTriggered(const std::string& command);
	static bool IsPressed(const std::string& command);
	static bool IsReleased(const std::string& command);

	static void BindKey(const std::string& key, const std::string& command); // キーにコマンドをバインド
	static void UnbindKey(const std::string& key); // キーのバインドを解除
	static void UnbindAll(); // すべてのバインドを解除
	static void ExecuteCommand(const std::string& command, bool isDown); // コマンドを実行

	static void ResetAllKeys();

private:
	static std::string GetKeyName(UINT virtualKey);

	static Vec2 mouseDelta_;

	struct CommandState {
		bool isTriggered = false; // トリガーされた
		bool isPressed = false; // 押されている
	};

	static std::unordered_map<std::string, CommandState> commandStates_; // コマンドの状態
	static std::unordered_map<std::string, std::string> keyBindings_; // キーとコマンドのバインド
	static std::unordered_map<std::string, bool> triggeredCommands_;
	static std::unordered_map<std::string, bool> pressedCommands_;
	static std::unordered_map<std::string, bool> releasedCommands_;
};
