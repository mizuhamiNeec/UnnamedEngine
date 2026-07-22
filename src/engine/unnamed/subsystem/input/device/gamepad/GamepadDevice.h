#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <array>
#include <unordered_map>
#include <vector>

#include "../base/BaseInputDevice.h"

enum : uint16_t {
	VG_A = 1000,
	VG_B,
	VG_X,
	VG_Y,
	VG_LB,
	VG_RB,
	VG_BACK,
	VG_START,
	VG_LS,
	VG_RS,
	VG_DPAD_UP,
	VG_DPAD_DOWN,
	VG_DPAD_LEFT,
	VG_DPAD_RIGHT,
	VG_LX,
	VG_LY,
	VG_RX,
	VG_RY,
	VG_LT,
	VG_RT,
	VG_LSTICK_LEFT,
	VG_LSTICK_RIGHT,
	VG_LSTICK_UP,
	VG_LSTICK_DOWN,
	VG_RSTICK_LEFT,
	VG_RSTICK_RIGHT,
	VG_RSTICK_UP,
	VG_RSTICK_DOWN,
};

namespace Unnamed {
	class GamepadDevice final : public BaseInputDevice {
	public:
		explicit GamepadDevice(HWND hWnd);
		~GamepadDevice() override;

		void Update() override;
		[[nodiscard]] bool GetKeyState(const InputKey& key) const override;
		[[nodiscard]] float GetAnalogValue(const InputKey& key) const override;
		[[nodiscard]] std::vector<InputKey> GetSupportedKeys() const override;
		[[nodiscard]] InputDeviceType GetDeviceType() const override;
		void ResetStates() override;

		void EnqueueRumble(float low, float high, float durationSec);
		void StopRumble();
		void RequestDirectInputRefresh();

	private:
		using PFN_XInputGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
		using PFN_XInputSetState = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
		using PFN_XInputGetCapabilities =
		DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

		struct DirectInputPad {
			IDirectInputDevice8W* device = nullptr;
		};

		struct RumbleEvent {
			float  low     = 0.0f;
			float  high    = 0.0f;
			double endTime = 0.0;
		};

		static BOOL CALLBACK EnumerateDevicesThunk(
			const DIDEVICEINSTANCEW* instance,
			VOID*                    context
		);
		BOOL EnumerateDevice(const DIDEVICEINSTANCEW* instance);

		static BOOL CALLBACK ConfigureAxisRangeThunk(
			const DIDEVICEOBJECTINSTANCEW* object,
			VOID*                          context
		);
		BOOL ConfigureAxisRange(const DIDEVICEOBJECTINSTANCEW* object);

		void               LoadXInput();
		void               UnloadXInput();
		[[nodiscard]] bool PollXInput();
		void               MergeXInputState(
			uint32_t            userIndex,
			const XINPUT_STATE& state
		);
		void RefreshXInputRumbleTarget(
			uint32_t            userIndex,
			const XINPUT_STATE& state
		);

		void               LoadDirectInput();
		void               UnloadDirectInput();
		void               EnumerateDirectInputDevices();
		void               ClearDirectInputDevices();
		[[nodiscard]] bool PollDirectInput();
		void               MergeDirectInputState(const DIJOYSTATE2& state);

		void ResetFrameStates();
		void UpdateDirectionalButtonsFromAnalogs();
		void ApplyRumble();
		void ApplyRumbleToXInput(
			uint32_t userIndex, float low, float high
		) const;

		[[nodiscard]] static float NormalizeSignedAxis(int value, int deadzone);
		[[nodiscard]] static float NormalizeUnsignedAxis(
			uint32_t value,
			uint32_t deadzone
		);
		[[nodiscard]] static float  Clamp01(float value);
		[[nodiscard]] static double NowSeconds();
		[[nodiscard]] static bool   IsAnalogCode(uint32_t code);
		[[nodiscard]] static bool   IsStickCode(uint32_t code);

		HWND mWindowHandle = nullptr;

		std::unordered_map<uint32_t, bool>  mButtonStates;
		std::unordered_map<uint32_t, float> mAnalogStates;

		HMODULE                   mXInputModule            = nullptr;
		PFN_XInputGetState        mXInputGetState          = nullptr;
		PFN_XInputSetState        mXInputSetState          = nullptr;
		PFN_XInputGetCapabilities mXInputGetCapabilities   = nullptr;
		std::array<bool, 4>       mXInputConnected         = {};
		uint32_t                  mLastActiveXInputUser    = 0;
		bool                      mHasLastActiveXInputUser = false;

		HMODULE                     mDirectInputModule            = nullptr;
		IDirectInput8W*             mDirectInput                  = nullptr;
		IDirectInputDevice8W*       mConfiguringDirectInputDevice = nullptr;
		std::vector<DirectInputPad> mDirectInputPads;
		bool                        mDirectInputRefreshRequested = false;

		std::vector<RumbleEvent> mRumbleEvents;
		float                    mAppliedRumbleLow  = 0.0f;
		float                    mAppliedRumbleHigh = 0.0f;

		static constexpr float    kAxisAsButtonThreshold      = 0.25f;
		static constexpr float    kTriggerAsButtonThreshold   = 0.2f;
		static constexpr float    kDirectInputStickDeadzone   = 0.12f;
		static constexpr float    kDirectInputTriggerDeadzone = 0.06f;
		static constexpr int      kXInputStickDeadzoneLeft    = 7849;
		static constexpr int      kXInputStickDeadzoneRight   = 8689;
		static constexpr uint32_t kXInputTriggerDeadzone      = 30;
	};
}
