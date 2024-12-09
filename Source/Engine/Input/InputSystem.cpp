#include "InputSystem.h"

#include <Windows.h>

#include "../Lib/Console/ConCommand.h"

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

	BYTE* lpb = new BYTE[dwSize];
	if (lpb == nullptr) {
		return;
	}

	if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
		delete[] lpb;
		return;
	}

	RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb);

	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		const RAWKEYBOARD& keyboard = raw->data.keyboard;
		UINT virtualKey = keyboard.VKey;
		bool isKeyDown = !(keyboard.Flags & RI_KEY_BREAK); // 押下: true, 離す: false

		std::string keyName = GetKeyName(virtualKey);
		if (keyBindings_.contains(keyName)) {
			ExecuteCommand(keyBindings_[keyName], isKeyDown);
		}
	}

	if (raw->header.dwType == RIM_TYPEMOUSE) {
		mouseDelta_.x += static_cast<float>(raw->data.mouse.lLastX);
		mouseDelta_.y += static_cast<float>(raw->data.mouse.lLastY);
	}

	delete[] lpb;
}

void InputSystem::Update() {
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
	auto it = commandStates_.find(command);
	return (it != commandStates_.end() && it->second.isTriggered);
}

bool InputSystem::IsPressed(const std::string& command) {
	auto it = commandStates_.find(command);
	return (it != commandStates_.end() && it->second.isPressed);
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

void InputSystem::ExecuteCommand(const std::string& command, bool isPressed) {
	CommandState& state = commandStates_[command];
	if (isPressed) {
		if (!state.isTriggered) { // 初めて押された場合
			state.isPressed = true; // このフレームのみのPress状態をONにする
		}
		state.isTriggered = true;   // 押下中状態をONにする
	} else {
		state.isTriggered = false;  // 離されたのでTrigger状態をOFF
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