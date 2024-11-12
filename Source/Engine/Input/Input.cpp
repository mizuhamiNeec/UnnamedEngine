#include "Input.h"

#include <cassert>

#include "../Window/Window.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

Input* Input::GetInstance() {
	static Input instance; // シングルトンインスタンス
	return &instance;	   // ポインタを返す
}

void Input::Init(const Window* window) {
	ComPtr<IDirectInput8> directInput = nullptr;
	HRESULT hr = DirectInput8Create(
		window->GetHInstance(),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(directInput.GetAddressOf()), nullptr);
	assert(SUCCEEDED(hr));

	// キーボードデバイスの生成
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成
	hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard->SetCooperativeLevel(
		window->GetWindowHandle(),
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
}

void Input::Update() {
	memcpy(preKey, key, sizeof(key));

	keyboard->Acquire();

	keyboard->GetDeviceState(sizeof(key), key);

	mouse->Acquire();
	mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
}

bool Input::PushKey(const BYTE keyNumber) const {
	// 指定キーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}
	// そうでなければfalseを返す
	return false;
}

bool Input::TriggerKey(const BYTE keyNumber) const {
	if (key[keyNumber] && !preKey[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::PushMouseButton(const int button) const {
	// 指定されたボタンが押されていればtrueを返す
	return (mouseState.rgbButtons[button] & 0x80) != 0;
}

bool Input::TriggerMouseButton(const int button) const {
	// ボタンが押された瞬間を検出するためのロジックを追加
	static BYTE previousButtons[3] = { 0, 0, 0 }; // 前回のボタン状態を保持

	bool isTriggered = (mouseState.rgbButtons[button] & 0x80) && !(previousButtons[button] & 0x80);
	previousButtons[button] = mouseState.rgbButtons[button]; // 現在の状態を保存

	return isTriggered;
}

void Input::GetMouseDelta(int& deltaX, int& deltaY) const {
	deltaX = mouseState.lX; // X方向のデルタ
	deltaY = mouseState.lY; // Y方向のデルタ
}