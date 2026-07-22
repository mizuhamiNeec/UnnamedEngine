#include "GamepadDevice.h"

#include <pch.h>

#include <algorithm>
#include <chrono>
#include <ranges>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace Unnamed {
	namespace {
		constexpr std::array<const char*, 5> kXInputDllNames = {
			"xinput1_4.dll",
			"xinput1_3.dll",
			"xinput9_1_0.dll",
			"xinput1_2.dll",
			"xinput1_1.dll",
		};

		constexpr std::array<InputKey, 28> kSupportedKeys = {
			InputKey{InputDeviceType::GAMEPAD, VG_A},
			InputKey{InputDeviceType::GAMEPAD, VG_B},
			InputKey{InputDeviceType::GAMEPAD, VG_X},
			InputKey{InputDeviceType::GAMEPAD, VG_Y},
			InputKey{InputDeviceType::GAMEPAD, VG_LB},
			InputKey{InputDeviceType::GAMEPAD, VG_RB},
			InputKey{InputDeviceType::GAMEPAD, VG_BACK},
			InputKey{InputDeviceType::GAMEPAD, VG_START},
			InputKey{InputDeviceType::GAMEPAD, VG_LS},
			InputKey{InputDeviceType::GAMEPAD, VG_RS},
			InputKey{InputDeviceType::GAMEPAD, VG_DPAD_UP},
			InputKey{InputDeviceType::GAMEPAD, VG_DPAD_DOWN},
			InputKey{InputDeviceType::GAMEPAD, VG_DPAD_LEFT},
			InputKey{InputDeviceType::GAMEPAD, VG_DPAD_RIGHT},
			InputKey{InputDeviceType::GAMEPAD, VG_LX},
			InputKey{InputDeviceType::GAMEPAD, VG_LY},
			InputKey{InputDeviceType::GAMEPAD, VG_RX},
			InputKey{InputDeviceType::GAMEPAD, VG_RY},
			InputKey{InputDeviceType::GAMEPAD, VG_LT},
			InputKey{InputDeviceType::GAMEPAD, VG_RT},
			InputKey{InputDeviceType::GAMEPAD, VG_LSTICK_LEFT},
			InputKey{InputDeviceType::GAMEPAD, VG_LSTICK_RIGHT},
			InputKey{InputDeviceType::GAMEPAD, VG_LSTICK_UP},
			InputKey{InputDeviceType::GAMEPAD, VG_LSTICK_DOWN},
			InputKey{InputDeviceType::GAMEPAD, VG_RSTICK_LEFT},
			InputKey{InputDeviceType::GAMEPAD, VG_RSTICK_RIGHT},
			InputKey{InputDeviceType::GAMEPAD, VG_RSTICK_UP},
			InputKey{InputDeviceType::GAMEPAD, VG_RSTICK_DOWN},
		};

		float NormalizeDirectInputSignedAxis(const LONG value) {
			if (value >= -1000 && value <= 1000) {
				return std::clamp(static_cast<float>(value) / 1000.0f, -1.0f,
				                  1.0f);
			}

			constexpr float kCenter = 32767.5f;
			return std::clamp((static_cast<float>(value) - kCenter) / kCenter,
			                  -1.0f, 1.0f);
		}

		float NormalizeDirectInputUnsignedAxis(const LONG value) {
			if (value >= 0 && value <= 255) {
				return std::clamp(static_cast<float>(value) / 255.0f, 0.0f,
				                  1.0f);
			}

			if (value >= 0 && value <= 1000) {
				return std::clamp(static_cast<float>(value) / 1000.0f, 0.0f,
				                  1.0f);
			}

			return std::clamp(static_cast<float>(value) / 65535.0f, 0.0f, 1.0f);
		}

		float ApplySignedDeadzone(const float value, const float deadzone) {
			const float absValue = std::abs(value);
			if (absValue <= deadzone) {
				return 0.0f;
			}
			const float sign = value >= 0.0f ? 1.0f : -1.0f;
			return sign * std::clamp(
				       (absValue - deadzone) / (1.0f - deadzone),
				       0.0f,
				       1.0f
			       );
		}

		float ApplyUnsignedDeadzone(const float value, const float deadzone) {
			if (value <= deadzone) {
				return 0.0f;
			}
			return std::clamp(
				(value - deadzone) / (1.0f - deadzone),
				0.0f,
				1.0f
			);
		}

		bool HasXInputActivity(const XINPUT_STATE& state) {
			const auto& g = state.Gamepad;
			return g.wButtons != 0 ||
			       g.bLeftTrigger > 30 ||
			       g.bRightTrigger > 30 ||
			       std::abs(g.sThumbLX) > 7849 ||
			       std::abs(g.sThumbLY) > 7849 ||
			       std::abs(g.sThumbRX) > 8689 ||
			       std::abs(g.sThumbRY) > 8689;
		}
	}

	GamepadDevice::GamepadDevice(const HWND hWnd) : mWindowHandle(hWnd) {
		LoadXInput();
		LoadDirectInput();
		ResetFrameStates();
	}

	GamepadDevice::~GamepadDevice() {
		StopRumble();
		ClearDirectInputDevices();
		UnloadDirectInput();
		UnloadXInput();
	}

	void GamepadDevice::Update() {
		ResetFrameStates();

		const bool hasXInput = PollXInput();
		if (mDirectInputRefreshRequested) {
			EnumerateDirectInputDevices();
			mDirectInputRefreshRequested = false;
		}

		// XInput 対応パッドを優先し、同じデバイスの二重入力を避ける
		if (!hasXInput) {
			(void)PollDirectInput();
		}

		UpdateDirectionalButtonsFromAnalogs();
		ApplyRumble();
	}

	bool GamepadDevice::GetKeyState(const InputKey& key) const {
		if (key.device != InputDeviceType::GAMEPAD) {
			return false;
		}

		if (const auto it = mButtonStates.find(key.code);
			it != mButtonStates.end()) {
			return it->second;
		}

		if (const auto it = mAnalogStates.find(key.code);
			it != mAnalogStates.end()) {
			if (key.code == VG_LT || key.code == VG_RT) {
				return it->second > kTriggerAsButtonThreshold;
			}

			if (IsStickCode(key.code)) {
				return std::abs(it->second) > kAxisAsButtonThreshold;
			}
		}

		return false;
	}

	float GamepadDevice::GetAnalogValue(const InputKey& key) const {
		if (key.device != InputDeviceType::GAMEPAD) {
			return 0.0f;
		}

		if (const auto it = mAnalogStates.find(key.code);
			it != mAnalogStates.end()) {
			return it->second;
		}

		if (const auto it = mButtonStates.find(key.code);
			it != mButtonStates.end()) {
			return it->second ? 1.0f : 0.0f;
		}

		return 0.0f;
	}

	std::vector<InputKey> GamepadDevice::GetSupportedKeys() const {
		return {kSupportedKeys.begin(), kSupportedKeys.end()};
	}

	InputDeviceType GamepadDevice::GetDeviceType() const {
		return InputDeviceType::GAMEPAD;
	}

	void GamepadDevice::ResetStates() {
		ResetFrameStates();
		StopRumble();
	}

	void GamepadDevice::EnqueueRumble(
		const float low,
		const float high,
		const float durationSec
	) {
		if (durationSec <= 0.0f) {
			return;
		}

		mRumbleEvents.emplace_back(RumbleEvent{
			.low     = Clamp01(low),
			.high    = Clamp01(high),
			.endTime = NowSeconds() + static_cast<double>(durationSec)
		});
	}

	void GamepadDevice::StopRumble() {
		mRumbleEvents.clear();
		mAppliedRumbleLow  = 0.0f;
		mAppliedRumbleHigh = 0.0f;

		if (!mXInputSetState) {
			return;
		}

		for (DWORD user = 0; user < 4; ++user) {
			ApplyRumbleToXInput(user, 0.0f, 0.0f);
		}
	}

	void GamepadDevice::RequestDirectInputRefresh() {
		mDirectInputRefreshRequested = true;
	}

	BOOL CALLBACK GamepadDevice::EnumerateDevicesThunk(
		const DIDEVICEINSTANCEW* instance,
		VOID*                    context
	) {
		auto* self = static_cast<GamepadDevice*>(context);
		if (!self) {
			return DIENUM_STOP;
		}
		return self->EnumerateDevice(instance) ? DIENUM_CONTINUE : DIENUM_STOP;
	}

	BOOL GamepadDevice::EnumerateDevice(const DIDEVICEINSTANCEW* instance) {
		if (!mDirectInput || !instance) {
			return FALSE;
		}

		IDirectInputDevice8W* device = nullptr;
		if (FAILED(
			mDirectInput->CreateDevice(instance->guidInstance, &device, nullptr
			))) {
			return TRUE;
		}

		if (FAILED(device->SetDataFormat(&c_dfDIJoystick2))) {
			device->Release();
			return TRUE;
		}

		if (mWindowHandle) {
			device->SetCooperativeLevel(
				mWindowHandle,
				DISCL_BACKGROUND | DISCL_NONEXCLUSIVE
			);
		}

		mConfiguringDirectInputDevice = device;
		(void)device->EnumObjects(
			ConfigureAxisRangeThunk,
			this,
			DIDFT_AXIS
		);
		mConfiguringDirectInputDevice = nullptr;

		(void)device->Acquire();
		mDirectInputPads.push_back(DirectInputPad{.device = device});
		return TRUE;
	}

	BOOL CALLBACK GamepadDevice::ConfigureAxisRangeThunk(
		const DIDEVICEOBJECTINSTANCEW* object,
		VOID*                          context
	) {
		auto* self = static_cast<GamepadDevice*>(context);
		if (!self) {
			return DIENUM_STOP;
		}
		return self->ConfigureAxisRange(object);
	}

	BOOL GamepadDevice::ConfigureAxisRange(
		const DIDEVICEOBJECTINSTANCEW* object
	) {
		if (!object || !mConfiguringDirectInputDevice) {
			return DIENUM_CONTINUE;
		}

		DIPROPRANGE range       = {};
		range.diph.dwSize       = sizeof(range);
		range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		range.diph.dwHow        = DIPH_BYID;
		range.diph.dwObj        = object->dwType;
		range.lMin              = -1000;
		range.lMax              = 1000;

		(void)mConfiguringDirectInputDevice->SetProperty(
			DIPROP_RANGE,
			&range.diph
		);
		return DIENUM_CONTINUE;
	}

	void GamepadDevice::LoadXInput() {
		for (const char* dll : kXInputDllNames) {
			const HMODULE module = LoadLibraryA(dll);
			if (!module) {
				continue;
			}

			auto* getState = reinterpret_cast<PFN_XInputGetState>(
				GetProcAddress(module, "XInputGetState")
			);
			auto* setState = reinterpret_cast<PFN_XInputSetState>(
				GetProcAddress(module, "XInputSetState")
			);
			auto* getCaps = reinterpret_cast<PFN_XInputGetCapabilities>(
				GetProcAddress(module, "XInputGetCapabilities")
			);

			if (!getState || !setState) {
				FreeLibrary(module);
				continue;
			}

			mXInputModule          = module;
			mXInputGetState        = getState;
			mXInputSetState        = setState;
			mXInputGetCapabilities = getCaps;
			return;
		}
	}

	void GamepadDevice::UnloadXInput() {
		if (mXInputModule) {
			FreeLibrary(mXInputModule);
			mXInputModule = nullptr;
		}

		mXInputGetState        = nullptr;
		mXInputSetState        = nullptr;
		mXInputGetCapabilities = nullptr;
	}

	bool GamepadDevice::PollXInput() {
		if (!mXInputGetState) {
			mXInputConnected.fill(false);
			return false;
		}

		bool anyConnected = false;
		for (DWORD user = 0; user < 4; ++user) {
			XINPUT_STATE state     = {};
			const DWORD  result    = mXInputGetState(user, &state);
			const bool   connected = result == ERROR_SUCCESS;
			mXInputConnected[user] = connected;
			if (!connected) {
				continue;
			}

			anyConnected = true;
			MergeXInputState(user, state);
			RefreshXInputRumbleTarget(user, state);
		}

		return anyConnected;
	}

	void GamepadDevice::MergeXInputState(
		const uint32_t      userIndex,
		const XINPUT_STATE& state
	) {
		(void)userIndex;
		const auto& gamepad = state.Gamepad;

		// 複数コントローラーを一つの仮想ゲームパッドとして合成する
		const auto mergeButton = [this](
			const uint32_t code, const bool pressed
		) {
			mButtonStates[code] = mButtonStates[code] || pressed;
		};
		const auto mergeAnalog = [this](
			const uint32_t code, const float value
		) {
			auto& slot = mAnalogStates[code];
			if (std::abs(value) > std::abs(slot)) {
				slot = value;
			}
		};

		mergeButton(VG_A, (gamepad.wButtons & XINPUT_GAMEPAD_A) != 0);
		mergeButton(VG_B, (gamepad.wButtons & XINPUT_GAMEPAD_B) != 0);
		mergeButton(VG_X, (gamepad.wButtons & XINPUT_GAMEPAD_X) != 0);
		mergeButton(VG_Y, (gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0);
		mergeButton(
			VG_LB, (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
		mergeButton(
			VG_RB, (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
		mergeButton(VG_BACK, (gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0);
		mergeButton(VG_START, (gamepad.wButtons & XINPUT_GAMEPAD_START) != 0);
		mergeButton(VG_LS, (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
		mergeButton(
			VG_RS, (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
		mergeButton(VG_DPAD_UP,
		            (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
		mergeButton(VG_DPAD_DOWN,
		            (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
		mergeButton(VG_DPAD_LEFT,
		            (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
		mergeButton(VG_DPAD_RIGHT,
		            (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);

		mergeAnalog(
			VG_LX,
			NormalizeSignedAxis(gamepad.sThumbLX, kXInputStickDeadzoneLeft)
		);
		mergeAnalog(
			VG_LY,
			NormalizeSignedAxis(gamepad.sThumbLY, kXInputStickDeadzoneLeft)
		);
		mergeAnalog(
			VG_RX,
			NormalizeSignedAxis(gamepad.sThumbRX, kXInputStickDeadzoneRight)
		);
		mergeAnalog(
			VG_RY,
			NormalizeSignedAxis(gamepad.sThumbRY, kXInputStickDeadzoneRight)
		);
		mergeAnalog(
			VG_LT,
			NormalizeUnsignedAxis(gamepad.bLeftTrigger, kXInputTriggerDeadzone)
		);
		mergeAnalog(
			VG_RT,
			NormalizeUnsignedAxis(gamepad.bRightTrigger, kXInputTriggerDeadzone)
		);
	}

	void GamepadDevice::RefreshXInputRumbleTarget(
		const uint32_t      userIndex,
		const XINPUT_STATE& state
	) {
		if (!HasXInputActivity(state)) {
			return;
		}

		mLastActiveXInputUser    = userIndex;
		mHasLastActiveXInputUser = true;
	}

	void GamepadDevice::LoadDirectInput() {
		mDirectInputModule = LoadLibraryA("dinput8.dll");
		if (!mDirectInputModule) {
			return;
		}

		using PFN_DirectInput8Create = HRESULT(WINAPI*)(
			HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN
		);
		auto* directInput8Create = reinterpret_cast<PFN_DirectInput8Create>(
			GetProcAddress(mDirectInputModule, "DirectInput8Create")
		);
		if (!directInput8Create) {
			FreeLibrary(mDirectInputModule);
			mDirectInputModule = nullptr;
			return;
		}

		if (FAILED(directInput8Create(
			GetModuleHandleW(nullptr),
			DIRECTINPUT_VERSION,
			IID_IDirectInput8W,
			reinterpret_cast<void**>(&mDirectInput),
			nullptr
		))) {
			FreeLibrary(mDirectInputModule);
			mDirectInputModule = nullptr;
			return;
		}

		EnumerateDirectInputDevices();
		mDirectInputRefreshRequested = false;
	}

	void GamepadDevice::UnloadDirectInput() {
		ClearDirectInputDevices();

		if (mDirectInput) {
			mDirectInput->Release();
			mDirectInput = nullptr;
		}

		if (mDirectInputModule) {
			FreeLibrary(mDirectInputModule);
			mDirectInputModule = nullptr;
		}
	}

	void GamepadDevice::EnumerateDirectInputDevices() {
		if (!mDirectInput) {
			return;
		}

		ClearDirectInputDevices();
		(void)mDirectInput->EnumDevices(
			DI8DEVCLASS_GAMECTRL,
			EnumerateDevicesThunk,
			this,
			DIEDFL_ATTACHEDONLY
		);
	}

	void GamepadDevice::ClearDirectInputDevices() {
		for (const auto& pad : mDirectInputPads) {
			if (!pad.device) {
				continue;
			}

			pad.device->Unacquire();
			pad.device->Release();
		}
		mDirectInputPads.clear();
	}

	bool GamepadDevice::PollDirectInput() {
		if (mDirectInputPads.empty()) {
			return false;
		}

		bool anyDeviceActive = false;
		for (const auto& pad : mDirectInputPads) {
			if (!pad.device) {
				continue;
			}

			HRESULT hr = pad.device->Poll();
			if (FAILED(hr)) {
				hr = pad.device->Acquire();
				while (hr == DIERR_INPUTLOST) {
					hr = pad.device->Acquire();
				}

				if (
					hr == DIERR_NOTACQUIRED ||
					hr == DIERR_OTHERAPPHASPRIO
				) {
					continue;
				}

				hr = pad.device->Poll();
				if (FAILED(hr)) {
					continue;
				}
			}

			DIJOYSTATE2 state = {};
			if (FAILED(pad.device->GetDeviceState(sizeof(state), &state))) {
				continue;
			}

			anyDeviceActive = true;
			MergeDirectInputState(state);
		}

		return anyDeviceActive;
	}

	void GamepadDevice::MergeDirectInputState(const DIJOYSTATE2& state) {
		const auto mergeButton = [this](
			const uint32_t code, const bool pressed
		) {
			mButtonStates[code] = mButtonStates[code] || pressed;
		};
		const auto mergeAnalog = [this](
			const uint32_t code, const float value
		) {
			auto& slot = mAnalogStates[code];
			if (std::abs(value) > std::abs(slot)) {
				slot = value;
			}
		};

		mergeButton(VG_A, (state.rgbButtons[0] & 0x80) != 0);
		mergeButton(VG_B, (state.rgbButtons[1] & 0x80) != 0);
		mergeButton(VG_X, (state.rgbButtons[2] & 0x80) != 0);
		mergeButton(VG_Y, (state.rgbButtons[3] & 0x80) != 0);
		mergeButton(VG_LB, (state.rgbButtons[4] & 0x80) != 0);
		mergeButton(VG_RB, (state.rgbButtons[5] & 0x80) != 0);
		mergeButton(VG_BACK, (state.rgbButtons[6] & 0x80) != 0);
		mergeButton(VG_START, (state.rgbButtons[7] & 0x80) != 0);
		mergeButton(VG_LS, (state.rgbButtons[8] & 0x80) != 0);
		mergeButton(VG_RS, (state.rgbButtons[9] & 0x80) != 0);

		bool hasPov = false;
		for (const DWORD pov : state.rgdwPOV) {
			if (pov == 0xFFFFFFFF) {
				continue;
			}
			hasPov = true;
			mergeButton(VG_DPAD_UP, pov >= 31500 || pov <= 4500);
			mergeButton(VG_DPAD_RIGHT, pov >= 4500 && pov <= 13500);
			mergeButton(VG_DPAD_DOWN, pov >= 13500 && pov <= 22500);
			mergeButton(VG_DPAD_LEFT, pov >= 22500 && pov <= 31500);
		}

		if (!hasPov) {
			mergeButton(VG_DPAD_UP, (state.rgbButtons[10] & 0x80) != 0);
			mergeButton(VG_DPAD_DOWN, (state.rgbButtons[11] & 0x80) != 0);
			mergeButton(VG_DPAD_LEFT, (state.rgbButtons[12] & 0x80) != 0);
			mergeButton(VG_DPAD_RIGHT, (state.rgbButtons[13] & 0x80) != 0);
		}

		const float lx = ApplySignedDeadzone(
			NormalizeDirectInputSignedAxis(state.lX),
			kDirectInputStickDeadzone
		);
		const float ly = ApplySignedDeadzone(
			-NormalizeDirectInputSignedAxis(state.lY),
			kDirectInputStickDeadzone
		);

		float rx = ApplySignedDeadzone(
			NormalizeDirectInputSignedAxis(state.lRx),
			kDirectInputStickDeadzone
		);
		float ry = ApplySignedDeadzone(
			-NormalizeDirectInputSignedAxis(state.lRy),
			kDirectInputStickDeadzone
		);
		if (std::abs(rx) < 0.01f && std::abs(ry) < 0.01f) {
			rx = ApplySignedDeadzone(
				NormalizeDirectInputSignedAxis(state.lZ),
				kDirectInputStickDeadzone
			);
			ry = ApplySignedDeadzone(
				-NormalizeDirectInputSignedAxis(state.lRz),
				kDirectInputStickDeadzone
			);
		}

		float       lt      = 0.0f;
		float       rt      = 0.0f;
		const float slider0 = NormalizeDirectInputUnsignedAxis(
			state.rglSlider[0]);
		const float slider1 = NormalizeDirectInputUnsignedAxis(
			state.rglSlider[1]);
		if (slider0 > 0.01f || slider1 > 0.01f) {
			lt = ApplyUnsignedDeadzone(slider0, kDirectInputTriggerDeadzone);
			rt = ApplyUnsignedDeadzone(slider1, kDirectInputTriggerDeadzone);
		} else {
			const float zCombined = NormalizeDirectInputSignedAxis(state.lZ);
			lt                    = ApplyUnsignedDeadzone(
				std::max(0.0f, -zCombined),
				kDirectInputTriggerDeadzone
			);
			rt = ApplyUnsignedDeadzone(
				std::max(0.0f, zCombined),
				kDirectInputTriggerDeadzone
			);
		}

		mergeAnalog(VG_LX, lx);
		mergeAnalog(VG_LY, ly);
		mergeAnalog(VG_RX, rx);
		mergeAnalog(VG_RY, ry);
		mergeAnalog(VG_LT, lt);
		mergeAnalog(VG_RT, rt);
	}

	void GamepadDevice::ResetFrameStates() {
		mButtonStates.clear();
		mAnalogStates.clear();

		for (const auto& key : kSupportedKeys) {
			if (IsAnalogCode(key.code)) {
				mAnalogStates.emplace(key.code, 0.0f);
			} else {
				mButtonStates.emplace(key.code, false);
			}
		}
	}

	void GamepadDevice::UpdateDirectionalButtonsFromAnalogs() {
		const float lx = mAnalogStates[VG_LX];
		const float ly = mAnalogStates[VG_LY];
		const float rx = mAnalogStates[VG_RX];
		const float ry = mAnalogStates[VG_RY];

		mButtonStates[VG_LSTICK_LEFT]  = lx < -kAxisAsButtonThreshold;
		mButtonStates[VG_LSTICK_RIGHT] = lx > kAxisAsButtonThreshold;
		mButtonStates[VG_LSTICK_UP]    = ly > kAxisAsButtonThreshold;
		mButtonStates[VG_LSTICK_DOWN]  = ly < -kAxisAsButtonThreshold;

		mButtonStates[VG_RSTICK_LEFT]  = rx < -kAxisAsButtonThreshold;
		mButtonStates[VG_RSTICK_RIGHT] = rx > kAxisAsButtonThreshold;
		mButtonStates[VG_RSTICK_UP]    = ry > kAxisAsButtonThreshold;
		mButtonStates[VG_RSTICK_DOWN]  = ry < -kAxisAsButtonThreshold;
	}

	void GamepadDevice::ApplyRumble() {
		if (!mXInputSetState) {
			return;
		}

		const double now = NowSeconds();
		std::erase_if(
			mRumbleEvents, [now](const RumbleEvent& event) {
				return event.endTime <= now;
			}
		);

		float low  = 0.0f;
		float high = 0.0f;
		for (const auto& event : mRumbleEvents) {
			low  += event.low;
			high += event.high;
		}

		low  = Clamp01(low);
		high = Clamp01(high);

		if (
			std::abs(low - mAppliedRumbleLow) < 0.0001f &&
			std::abs(high - mAppliedRumbleHigh) < 0.0001f
		) {
			return;
		}

		mAppliedRumbleLow  = low;
		mAppliedRumbleHigh = high;

		// 振動は最後に操作された XInput パッドを優先する
		if (mHasLastActiveXInputUser &&
		    mXInputConnected[mLastActiveXInputUser]) {
			ApplyRumbleToXInput(mLastActiveXInputUser, low, high);
			return;
		}

		for (uint32_t user = 0; user < 4; ++user) {
			if (!mXInputConnected[user]) {
				continue;
			}
			ApplyRumbleToXInput(user, low, high);
			break;
		}
	}

	void GamepadDevice::ApplyRumbleToXInput(
		const uint32_t userIndex,
		const float    low,
		const float    high
	) const {
		if (!mXInputSetState) {
			return;
		}

		XINPUT_VIBRATION vibration = {};
		vibration.wLeftMotorSpeed  = static_cast<WORD>(
			Clamp01(low) * 65535.0f
		);
		vibration.wRightMotorSpeed = static_cast<WORD>(
			Clamp01(high) * 65535.0f
		);
		(void)mXInputSetState(userIndex, &vibration);
	}

	float GamepadDevice::NormalizeSignedAxis(
		const int value,
		const int deadzone
	) {
		const int absValue = std::abs(value);
		if (absValue <= deadzone) {
			return 0.0f;
		}

		const float sign       = value >= 0 ? 1.0f : -1.0f;
		const float max        = value >= 0 ? 32767.0f : 32768.0f;
		const float normalized = static_cast<float>(absValue - deadzone) /
		                         (max - static_cast<float>(deadzone));
		return sign * std::clamp(normalized, 0.0f, 1.0f);
	}

	float GamepadDevice::NormalizeUnsignedAxis(
		const uint32_t value,
		const uint32_t deadzone
	) {
		const uint32_t maxValue = value <= 255 ? 255 : 65535;
		if (value <= deadzone || maxValue <= deadzone) {
			return 0.0f;
		}

		return std::clamp(
			static_cast<float>(value - deadzone) /
			static_cast<float>(maxValue - deadzone),
			0.0f,
			1.0f
		);
	}

	float GamepadDevice::Clamp01(const float value) {
		return std::clamp(value, 0.0f, 1.0f);
	}

	double GamepadDevice::NowSeconds() {
		using clock = std::chrono::steady_clock;
		return std::chrono::duration<double>(
			clock::now().time_since_epoch()
		).count();
	}

	bool GamepadDevice::IsAnalogCode(const uint32_t code) {
		return code == VG_LX || code == VG_LY ||
		       code == VG_RX || code == VG_RY ||
		       code == VG_LT || code == VG_RT;
	}

	bool GamepadDevice::IsStickCode(const uint32_t code) {
		return code == VG_LX || code == VG_LY ||
		       code == VG_RX || code == VG_RY;
	}
}
