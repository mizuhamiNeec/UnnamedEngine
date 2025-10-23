#include "KeyboardDevice.h"

#include <ranges>

namespace Unnamed {
	/// @brief コンストラクタ
	/// @param hWnd ウィンドウハンドル
	KeyboardDevice::KeyboardDevice(const HWND hWnd) {
		RAWINPUTDEVICE keyboardRid;
		keyboardRid.usUsagePage = 0x01;
		keyboardRid.usUsage     = 0x06; // キーボード
		keyboardRid.dwFlags     = 0;
		keyboardRid.hwndTarget  = hWnd;
		RegisterRawInputDevices(&keyboardRid, 1, sizeof(keyboardRid));
	}

	/// @brief デストラクタ
	KeyboardDevice::~KeyboardDevice() = default;

	/// @brief 生の入力を処理する
	/// @param raw 生の入力データ
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

	/// @brief デバイスの状態を更新する
	void KeyboardDevice::Update() {
	}

	/// @brief 指定したキーの状態を取得する
	/// @param key 入力キー
	/// @return キーが押されているかどうか
	bool KeyboardDevice::GetKeyState(const InputKey& key) const {
		if (key.device != InputDeviceType::KEYBOARD) {
			return false;
		}
		const auto it = mKeyStates.find(key.code);
		return it != mKeyStates.end() ? it->second : false;
	}

	/// @brief 指定したキーのアナログ値を取得する
	/// @param key 入力キー
	/// @return アナログ値
	float KeyboardDevice::GetAnalogValue(const InputKey& key) const {
		if (key.device != InputDeviceType::KEYBOARD) {
			return 0.0f;
		}
		const auto it = mKeyStates.find(key.code);
		return it != mKeyStates.end() && it->second ? 1.0f : 0.0f;
	}

	/// @brief サポートされているキーのリストを取得する
	/// @return サポートされているキーのベクター
	std::vector<InputKey> KeyboardDevice::GetSupportedKeys() const {
		std::vector<InputKey> supportedKeys;
		for (const auto& key : mKeyStates | std::views::keys) {
			supportedKeys.emplace_back(InputDeviceType::KEYBOARD, key);
		}
		return supportedKeys;
	}

	/// @brief デバイスタイプを取得する
	/// @return デバイスタイプ
	InputDeviceType KeyboardDevice::GetDeviceType() const {
		return InputDeviceType::KEYBOARD;
	}

	/// @brief キー状態をリセットする
	void KeyboardDevice::ResetStates() {
		mKeyStates.clear();
	}
}
