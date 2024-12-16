#include "InputSystem.h"

#include <Windows.h>

#include "../Lib/Console/ConCommand.h"
#include "../Lib/Console/Console.h"

void InputSystem::Initialize() {
	RAWINPUTDEVICE rid[2];

	// キーボードデバイスを登録
	rid[0].usUsagePage = 0x01;
	rid[0].usUsage = 0x06;
	rid[0].dwFlags = 0;
	rid[0].hwndTarget = nullptr;

	// マウスデバイスを登録
	rid[1].usUsagePage = 0x01;
	rid[1].usUsage = 0x02;
	rid[1].dwFlags = 0;
	rid[1].hwndTarget = nullptr;

	if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
		MessageBox(nullptr, L"Failed to register raw input devices", L"Error", MB_OK);
	}
}

void InputSystem::ProcessInput(const LPARAM lParam) {
	UINT dwSize = 0;
	GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

	auto* lpb = new BYTE[dwSize];
	if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
		delete[] lpb;
		return;
	}

	auto* raw = reinterpret_cast<RAWINPUT*>(lpb);

	// キーボード入力
	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		const auto vKey = raw->data.keyboard.VKey;
		const bool isKeyDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);

		// 仮想キーを文字列に変換
		std::string keyName = GetKeyName(vKey);
		keyName = StrUtils::ToLowerCase(keyName);
		if (!keyName.empty() && keyBindings_.contains(keyName)) {
			std::string cmd = keyBindings_[keyName];
			if (cmd[0] == '+') {
				std::string baseCmd = cmd.substr(1);
				if (isKeyDown) {
					triggeredCommands_[baseCmd] = true;
					pressedCommands_[baseCmd] = true;
				} else {
					pressedCommands_[baseCmd] = false;
					releasedCommands_[baseCmd] = true;
				}
			} else {
				// プレフィックスなしコマンドは押した瞬間のみ実行
				if (isKeyDown) {
					triggeredCommands_[cmd] = true;
					Console::SubmitCommand(cmd);
				}
			}
		}
	}
	// マウス入力
	else if (raw->header.dwType == RIM_TYPEMOUSE) {
		// マウスの移動量を更新
		mouseDelta_.x += static_cast<float>(raw->data.mouse.lLastX);
		mouseDelta_.y += static_cast<float>(raw->data.mouse.lLastY);

		// マウスボタンの状態を更新
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
			if (keyBindings_.contains("mouse1")) {
				std::string cmd = keyBindings_["mouse1"];
				if (cmd[0] == '+') {
					// +コマンドの場合、ベース名を抽出 (+attack -> attack)
					std::string baseCmd = cmd.substr(1);
					triggeredCommands_[baseCmd] = true;
					pressedCommands_[baseCmd] = true;  // 長押し状態を設定
				}
			}
		}

		if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
			if (keyBindings_.contains("mouse1")) {
				std::string cmd = keyBindings_["mouse1"];
				if (cmd[0] == '+') {
					std::string baseCmd = cmd.substr(1);
					pressedCommands_[baseCmd] = false;  // 長押し状態を解除
					releasedCommands_[baseCmd] = true;
				}
			}
		}

		if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
			if (keyBindings_.contains("mouse2")) {
				std::string cmd = keyBindings_["mouse2"];
				if (cmd[0] == '+') {
					// +コマンドの場合、ベース名を抽出 (+attack -> attack)
					std::string baseCmd = cmd.substr(1);
					triggeredCommands_[baseCmd] = true;
					pressedCommands_[baseCmd] = true;  // 長押し状態を設定
				}
			}
		}

		if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
			if (keyBindings_.contains("mouse2")) {
				std::string cmd = keyBindings_["mouse2"];
				if (cmd[0] == '+') {
					std::string baseCmd = cmd.substr(1);
					pressedCommands_[baseCmd] = false;  // 長押し状態を解除
					releasedCommands_[baseCmd] = true;
				}
			}
		}


		// マウスホイール
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
			short wheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
			if (wheelDelta > 0) {
				ExecuteCommand(keyBindings_["mousewheelup"], true);
				ExecuteCommand(keyBindings_["mousewheelup"], false);
			} else if (wheelDelta < 0) {
				ExecuteCommand(keyBindings_["mousewheeldown"], true);
				ExecuteCommand(keyBindings_["mousewheeldown"], false);
			}
		}
	}

	delete[] lpb;
}

void InputSystem::Update() {
	// トリガー/リリース状態のクリア
	triggeredCommands_.clear();
	releasedCommands_.clear();
	for (auto& [command, state] : commandStates_) {
		state.isPressed = false; // isPressedは毎フレームリセット
	}
}

Vec2 InputSystem::GetMouseDelta() {
	Vec2 delta = mouseDelta_;
	mouseDelta_ = Vec2::zero;
	return delta;
}

bool InputSystem::IsTriggered(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return triggeredCommands_[baseCommand];
}

bool InputSystem::IsPressed(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return pressedCommands_[baseCommand];
}

bool InputSystem::IsReleased(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return releasedCommands_[baseCommand];
}

void InputSystem::BindKey(const std::string& key, const std::string& command) {
	keyBindings_[key] = command;
}

void InputSystem::UnbindKey(const std::string& key) {
	keyBindings_.erase(key);
}

void InputSystem::UnbindAll() {
	keyBindings_.clear();
}

void InputSystem::ExecuteCommand(const std::string& command, bool isDown) {
	// +/- プレフィックスの解析
	std::string baseCommand = command;
	bool isPlusCommand = false;

	if (!command.empty()) {
		if (command[0] == '+') {
			isPlusCommand = true;
			baseCommand = command.substr(1);
		} else if (command[0] == '-') {
			isPlusCommand = false;
			baseCommand = command.substr(1);
		}
	}

	// コマンドの状態更新
	if (isPlusCommand) {
		// +プレフィックス付きコマンドは長押し状態を管理
		if (isDown) {
			pressedCommands_[baseCommand] = true;
			triggeredCommands_[baseCommand] = true;
		} else {
			pressedCommands_[baseCommand] = false;
			releasedCommands_[baseCommand] = true;
		}
	} else {
		// プレフィックスなしコマンドは押した瞬間のみ実行
		if (isDown) {
			triggeredCommands_[baseCommand] = true;
			// コンソールコマンドも実行
			Console::SubmitCommand(baseCommand);
		}
	}
}

std::string InputSystem::GetKeyName(const UINT virtualKey) {
	char name[256];
	if (GetKeyNameTextA(MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC) << 16, name, sizeof(name))) {
		return std::string(name);
	}
	return "Unknown";
}

Vec2 InputSystem::mouseDelta_ = Vec2::zero;

std::unordered_map<std::string, std::string> InputSystem::keyBindings_;
std::unordered_map<std::string, bool> InputSystem::keyStates_;
std::unordered_map<std::string, InputSystem::CommandState> InputSystem::commandStates_;
std::unordered_map<std::string, bool> InputSystem::triggeredCommands_;
std::unordered_map<std::string, bool> InputSystem::pressedCommands_;
std::unordered_map<std::string, bool> InputSystem::releasedCommands_;
