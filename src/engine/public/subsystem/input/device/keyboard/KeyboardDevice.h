#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include "engine/public/subsystem/input/device/base/BaseInputDevice.h"
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <unordered_map>

namespace Unnamed {
	class KeyboardDevice final : public BaseInputDevice {
	public:
		explicit KeyboardDevice(HWND hWnd);
		~KeyboardDevice() override;

		void HandleRawInput(const RAWINPUT& raw);

		void Update() override;
		[[nodiscard]] bool GetKeyState(const InputKey& key) const override;
		[[nodiscard]] float GetAnalogValue(const InputKey& key) const override;
		[[nodiscard]] std::vector<InputKey> GetSupportedKeys() const override;
		[[nodiscard]] InputDeviceType GetDeviceType() const override;
		void ResetStates() override;

	private:
		std::unordered_map<uint32_t, bool> mKeyStates;
	};
}
