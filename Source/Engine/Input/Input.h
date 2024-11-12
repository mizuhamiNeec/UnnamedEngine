#pragma once

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#include <wrl/client.h>

class Window;

class Input {
public:
	static Input* GetInstance();

	void Init(const Window* window);
	void Update();

	// キーの押下をチェック
	bool PushKey(BYTE keyNumber) const;
	// キーのトリガーをチェック
	bool TriggerKey(BYTE keyNumber) const;
	bool PushMouseButton(int button) const;
	bool TriggerMouseButton(int button) const;
	void GetMouseDelta(int& deltaX, int& deltaY) const;

public:
	// 全キーの状態
	BYTE key[256] = {};
	BYTE preKey[256] = {};

	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard = nullptr;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse = nullptr;

	DIMOUSESTATE mouseState;

	// コンストラクタをプライベートにして外部からのインスタンス化を防ぐ
	Input() = default;
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;
};