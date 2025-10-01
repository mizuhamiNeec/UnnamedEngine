#include "KeyboardDevice.h"

#include <ranges>

namespace Unnamed {
	KeyboardDevice::KeyboardDevice(const HWND hWnd) {
		RAWINPUTDEVICE keyboardRid;
		keyboardRid.usUsagePage = 0x01;
		keyboardRid.usUsage     = 0x06; // キーボード
		keyboardRid.dwFlags     = 0;
		keyboardRid.hwndTarget  = hWnd;
		RegisterRawInputDevices(&keyboardRid, 1, sizeof(keyboardRid));
	}

	KeyboardDevice::~KeyboardDevice() = default;

	void KeyboardDevice::HandleRawInput(const RAWINPUT& raw) {
		if (raw.header.dwType != RIM_TYPEKEYBOARD) {
			return;
		}

		const RAWKEYBOARD& keyboard = raw.data.keyboard;
		uint32_t           vk       = keyboard.VKey;

		// 右Ctrl、右Alt、右Shiftなど
		if (keyboard.Flags & RI_KEY_E0) {
			switch (vk) {
			case VK_CONTROL:
				vk = VK_RCONTROL;
				break;
			case VK_MENU:
				vk = VK_RMENU;
				break;
			case VK_SHIFT:
				vk = VK_RSHIFT;
				break;
			}
		} else {
			switch (vk) {
			case VK_CONTROL:
				vk = VK_LCONTROL;
				break;
			case VK_MENU:
				vk = VK_LMENU;
				break;
			case VK_SHIFT:
				vk = VK_LSHIFT;
				break;
			}
		}

		const bool bIsDown = !(keyboard.Flags & RI_KEY_BREAK);
		mKeyStates[vk]     = bIsDown;
	}

	void KeyboardDevice::Update() {
	}

	bool KeyboardDevice::GetKeyState(const InputKey& key) const {
		if (key.device != InputDeviceType::KEYBOARD) {
			return false;
		}
		const auto it = mKeyStates.find(key.code);
		return it != mKeyStates.end() ? it->second : false;
	}

	float KeyboardDevice::GetAnalogValue(const InputKey& key) const {
		if (key.device != InputDeviceType::KEYBOARD) {
			return 0.0f;
		}
		const auto it = mKeyStates.find(key.code);
		return it != mKeyStates.end() && it->second ? 1.0f : 0.0f;
	}

	std::vector<InputKey> KeyboardDevice::GetSupportedKeys() const {
		std::vector<InputKey> supportedKeys;
		for (const auto& key : mKeyStates | std::views::keys) {
			supportedKeys.emplace_back(InputDeviceType::KEYBOARD, key);
		}
		return supportedKeys;
	}

	InputDeviceType KeyboardDevice::GetDeviceType() const {
		return InputDeviceType::KEYBOARD;
	}

	void KeyboardDevice::ResetStates() {
		mKeyStates.clear();
	}
}
