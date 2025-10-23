#pragma once
#include <pch.h>
#include <string>
#include <unordered_map>

#include <runtime/core/math/Math.h>

/**
 * @brief 入力システムクラス
 * @details キーボードとマウスの入力を管理し、コマンドバインディング機能を提供します
 */
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
	static void UnbindKey(const std::string& key);
	static void UnbindAll();
	static void ExecuteCommand(const std::string& command, bool isDown);
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
