#pragma once

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#include "Source/Engine/Window/Window.h"

#include <wrl/client.h>

class Input {
public:
	void Setup(const Window& window);
	void Update();

	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard = nullptr;
};

