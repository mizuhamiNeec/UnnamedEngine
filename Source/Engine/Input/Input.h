#pragma once

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#include <wrl/client.h>

class Window;

class Input {
public:
	void Init(const Window* window);
	void Update();

	// キーの押下をチェック
	bool PushKey(BYTE keyNumber) const;
	// キーのトリガーをチェック
	bool TriggerKey(BYTE keyNumber) const;

public:
	// 全キーの状態
	BYTE key[256] = {};
	BYTE preKey[256] = {};

	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard = nullptr;
};

