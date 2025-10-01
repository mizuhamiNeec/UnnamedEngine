#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <unordered_map>
#include <Windows.h>

#include <engine/public/subsystem/input/device/base/BaseInputDevice.h>

enum : uint8_t {
	VM_1     = VK_LBUTTON,  // 左ボタン
	VM_2     = VK_RBUTTON,  // 右ボタン
	VM_3     = VK_MBUTTON,  // 中央ボタン
	VM_4     = VK_XBUTTON1, // サイドボタン1 手前
	VM_5     = VK_XBUTTON2, // サイドボタン2 奥
	VM_X     = 100,         // カーソルX軸
	VM_Y     = 101,         // カーソルY軸
	VM_WHEEL = 102,         // ホイール
};

namespace Unnamed {
	class MouseDevice : public BaseInputDevice {
	public:
		explicit MouseDevice(HWND hWnd);
		~MouseDevice() override;

		void Update() override;

		void HandleRawInput(const RAWINPUT& raw);

		[[nodiscard]] bool GetKeyState(const InputKey& key) const override;
		[[nodiscard]] float GetAnalogValue(const InputKey& key) const override;
		[[nodiscard]] std::vector<InputKey> GetSupportedKeys() const override;
		[[nodiscard]] InputDeviceType GetDeviceType() const override;
		void ResetStates() override;

		[[nodiscard]] int GetDeltaX() const { return mDeltaX; }
		[[nodiscard]] int GetDeltaY() const { return mDeltaY; }
		[[nodiscard]] int GetWheelDelta() const { return mWheelDelta; }

		void ResetDelta();

	private:
		std::unordered_map<uint32_t, bool> mButtonStates;
		int                                mDeltaX     = 0;
		int                                mDeltaY     = 0;
		int                                mWheelDelta = 0;
	};
}
