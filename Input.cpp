#include "Input.h"

#include <cassert>
#include <wrl/client.h>

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#include "Source/Engine/Window/Window.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

void Input::Setup(const Window& window) {
	ComPtr<IDirectInput8> directInput = nullptr;
	HRESULT hr = DirectInput8Create(
		GetModuleHandle(nullptr),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(directInput.GetAddressOf()), nullptr
	);
	assert(SUCCEEDED(hr));

	// キーボードデバイスの生成
	ComPtr<IDirectInputDevice8> keyboard = nullptr;
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard->SetCooperativeLevel(
		window.GetHWND(),
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY
	);
	assert(SUCCEEDED(hr));
}

void Input::Update() {
}
