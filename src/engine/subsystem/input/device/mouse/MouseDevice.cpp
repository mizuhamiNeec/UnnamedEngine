#include "MouseDevice.h"


namespace Unnamed {
	MouseDevice::MouseDevice(const HWND hWnd) {
		RAWINPUTDEVICE rid;
		rid.usUsagePage = 0x01;
		rid.usUsage     = 0x02; // マウス
		// ウィンドウ外のマウス移動を取得する
		rid.dwFlags    = 0; // RIDEV_INPUTSINK;
		rid.hwndTarget = hWnd;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
	}

	MouseDevice::~MouseDevice() = default;

	void MouseDevice::Update() {
	}

	void MouseDevice::HandleRawInput(const RAWINPUT& raw) {
		if (raw.header.dwType != RIM_TYPEMOUSE) {
			return;
		}
		const auto& mouseData = raw.data.mouse;

		// 第5ボタンまでの状態を取得 マウスの形をしたキーボードは知らん
		if (mouseData.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
			mButtonStates[VK_LBUTTON] = true;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
			mButtonStates[VK_LBUTTON] = false;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
			mButtonStates[VK_RBUTTON] = true;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
			mButtonStates[VK_RBUTTON] = false;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
			mButtonStates[VK_MBUTTON] = true;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
			mButtonStates[VK_MBUTTON] = false;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) {
			mButtonStates[VK_XBUTTON1] = true;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_BUTTON_4_UP) {
			mButtonStates[VK_XBUTTON1] = false;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) {
			mButtonStates[VK_XBUTTON2] = true;
		}
		if (mouseData.usButtonFlags & RI_MOUSE_BUTTON_5_UP) {
			mButtonStates[VK_XBUTTON2] = false;
		}

		// 移動量
		mDeltaX += mouseData.lLastX;
		mDeltaY += mouseData.lLastY;
		// ホイール
		if (mouseData.usButtonFlags & RI_MOUSE_WHEEL) {
			const SHORT wheelData = static_cast<SHORT>(mouseData.usButtonData);
			mWheelRemainder += wheelData;
			if (wheelData > 0) {
				mButtonStates[VM_WHEEL_UP]   = true;
				mButtonStates[VM_WHEEL_DOWN] = false;
			} else if (wheelData < 0) {
				mButtonStates[VM_WHEEL_DOWN] = true;
				mButtonStates[VM_WHEEL_UP]   = false;
			}
			const int clicks = mWheelRemainder / WHEEL_DELTA;
			if (clicks != 0) {
				mWheelDelta += clicks;
				mWheelRemainder -= clicks * WHEEL_DELTA;
			}
		}
	}

	bool MouseDevice::GetKeyState(const InputKey& key) const {
		if (key.device != InputDeviceType::MOUSE) {
			return false;
		}
		const auto it = mButtonStates.find(key.code);
		return it != mButtonStates.end() ? it->second : false;
	}

	float MouseDevice::GetAnalogValue(const InputKey& key) const {
		if (key.device != InputDeviceType::MOUSE) {
			return 0.0f;
		}
		if (key.code == VM_X) {
			return static_cast<float>(mDeltaX);
		}
		if (key.code == VM_Y) {
			return static_cast<float>(mDeltaY);
		}
		if (key.code == VM_WHEEL) {
			return static_cast<float>(mWheelDelta);
		}
		return 0.0f;
	}

	std::vector<InputKey> MouseDevice::GetSupportedKeys() const {
		return {
			{.device = InputDeviceType::MOUSE, .code = VM_1},
			{.device = InputDeviceType::MOUSE, .code = VM_2},
			{.device = InputDeviceType::MOUSE, .code = VM_3},
			{.device = InputDeviceType::MOUSE, .code = VM_4},
			{.device = InputDeviceType::MOUSE, .code = VM_5},
			{.device = InputDeviceType::MOUSE, .code = VM_X},
			{.device = InputDeviceType::MOUSE, .code = VM_Y},
			{.device = InputDeviceType::MOUSE, .code = VM_WHEEL},
			{.device = InputDeviceType::MOUSE, .code = VM_WHEEL_UP},
			{.device = InputDeviceType::MOUSE, .code = VM_WHEEL_DOWN}
		};
	}

	InputDeviceType MouseDevice::GetDeviceType() const {
		return InputDeviceType::MOUSE;
	}

	void MouseDevice::ResetStates() {
		mButtonStates.clear();
		mDeltaX         = 0;
		mDeltaY         = 0;
		mWheelDelta     = 0;
		mWheelRemainder = 0;
	}

	void MouseDevice::ResetDelta() {
		mDeltaX     = 0;
		mDeltaY     = 0;
		mWheelDelta = 0;
		mButtonStates[VM_WHEEL_UP]   = false;
		mButtonStates[VM_WHEEL_DOWN] = false;
	}
}
