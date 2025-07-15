#pragma once
#include <intsafe.h>
#include <string>
#include <unordered_map>

#include "../Lib/Math/Vector/Vec2.h"

class InputSystem {
public:
	static void Init();
	static void Update();

	static void ProcessInput(long lParam);

	static Vec2 GetMouseDelta();

	static bool IsTriggered(const std::string& command);
	static bool IsPressed(const std::string& command);
	static bool IsReleased(const std::string& command);

	static void BindKey(const std::string& key, const std::string& command);
	// キーにコマンドをバインド
	static void UnbindKey(const std::string& key); // キーのバインドを解除
	static void UnbindAll();                       // すべてのバインドを解除
	static void ExecuteCommand(const std::string& command, bool isDown);
	// コマンドを実行

	static void ResetAllKeys();

	static void CheckMouseCursorLock();

private:
	static void UpdateMouseButtonState(USHORT             buttonFlags,
	                                   const std::string& buttonName,
	                                   USHORT             buttonDownFlag,
	                                   USHORT             buttonUpFlag);

	static std::string GetKeyName(UINT virtualKey);

	static Vec2 mOuseDelta;

	struct CommandState {
		bool isTriggered = false; // トリガーされた
		bool isPressed   = false; // 押されている
	};

	static std::unordered_map<std::string, CommandState> mCommandStates;
	// コマンドの状態
	static std::unordered_map<std::string, std::string> mKeyBindings;
	// キーとコマンドのバインド
	static std::unordered_map<std::string, bool> mTriggeredCommands;
	static std::unordered_map<std::string, bool> mPressedCommands;
	static std::unordered_map<std::string, bool> mReleasedCommands;
	static bool                                  mMouseLock;    // マウスのロック
	static bool                                  mCursorHidden; // カーソルが非表示か
};
