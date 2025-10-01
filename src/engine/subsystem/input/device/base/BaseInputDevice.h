#pragma once
#include <cstdint>
#include <vector>

namespace Unnamed {
	enum class InputDeviceType {
		KEYBOARD,
		MOUSE,
		GAMEPAD
	};

	struct InputKey {
		InputDeviceType device;
		uint32_t        code;

		bool operator==(const InputKey& other) const {
			return device == other.device && code == other.code;
		}
	};

	class BaseInputDevice {
	public:
		virtual      ~BaseInputDevice() = default;
		virtual void Update() = 0;

		[[nodiscard]]
		virtual bool GetKeyState(const InputKey& key) const = 0;
		[[nodiscard]]
		virtual float GetAnalogValue(const InputKey& key) const = 0;
		[[nodiscard]]
		virtual std::vector<InputKey> GetSupportedKeys() const = 0;
		[[nodiscard]]
		virtual InputDeviceType GetDeviceType() const = 0;

		virtual void ResetStates() = 0;
	};
}
