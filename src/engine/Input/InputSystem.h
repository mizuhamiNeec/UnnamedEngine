#pragma once
#include <pch.h>
#include <string>
#include <unordered_map>

#include <vector>
#include <chrono>

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
	static Vec2 GetMousePosition();
	static bool IsTriggered(const std::string& command);
	static bool IsPressed(const std::string& command);
	static bool IsReleased(const std::string& command);
	static void BindKey(const std::string& key, const std::string& command);
	static void UnbindKey(const std::string& key);
	static void UnbindAll();
	static void ExecuteCommand(const std::string& command, bool isDown);
	static void ResetAllKeys();
	static void CheckMouseCursorLock(HWND hwnd, int32_t x, int32_t y);


	// コントローラー関連
	static void AddVibration(
		int padIndex, float leftMotor, float rightMotor, float duration
	);
	static void  SetVibration(int padIndex, float leftMotor, float rightMotor);
	static Vec2  GetLeftStick(int padIndex);
	static Vec2  GetRightStick(int padIndex);
	static float GetLeftTrigger(int padIndex);
	static float GetRightTrigger(int padIndex);

private:
	struct VibrationEffect {
		float leftMotor  = 0.0f;
		float rightMotor = 0.0f;
		float duration   = 0.0f;
	};

	static void UpdateMouseButtonState(
		USHORT             buttonFlags,
		const std::string& buttonName,
		USHORT             buttonDownFlag,
		USHORT             buttonUpFlag
	);

	static void UpdateGamepadButtonState(
		WORD               oldButtons,
		WORD               newButtons,
		WORD               targetFlag,
		const std::string& buttonName
	);

	static std::string GetKeyName(UINT virtualKey);

	static Vec2 mMouseDelta;
	static Vec2 mMousePosition;

	struct CommandState {
		bool isTriggered = false; // トリガーされた
		bool isPressed   = false; // 押されている
	};

	struct GamepadState {
		bool  isConnected  = false;
		float leftStickX   = 0.0f;
		float leftStickY   = 0.0f;
		float rightStickX  = 0.0f;
		float rightStickY  = 0.0f;
		float leftTrigger  = 0.0f;
		float rightTrigger = 0.0f;
		WORD  buttons      = 0;
	};

	static std::unordered_map<std::string, CommandState> mCommandStates;
	// コマンドの状態
	static std::unordered_map<std::string, std::string> mKeyBindings;
	// キーとコマンドのバインド
	static std::unordered_map<std::string, bool> mTriggeredCommands;
	static std::unordered_map<std::string, bool> mPressedCommands;
	static std::unordered_map<std::string, bool> mReleasedCommands;
	static GamepadState mGamepadStates[4];
	static std::vector<VibrationEffect> mActiveVibrations[4];
	static std::chrono::steady_clock::time_point mLastUpdateTime;
	static bool mMouseLock; // マウスのロック
	static bool mCursorHidden; // カーソルが非表示か
};
