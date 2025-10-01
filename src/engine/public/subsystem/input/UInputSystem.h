#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <engine/public/IWin32MsgListener.h>
#include <engine/public/subsystem/input/device/base/BaseInputDevice.h>

#include <runtime/core/math/Math.h>

#include <engine/public/subsystem/interface/ISubsystem.h>

template <>
struct std::hash<Unnamed::InputKey> {
	size_t operator()(const Unnamed::InputKey& key) const noexcept {
		return hash<uint32_t>()(static_cast<uint32_t>(key.device)) ^ hash<
			uint32_t>()(key.code << 1);
	}
};

namespace Unnamed {
	struct InputActionState {
		bool bIsPressed  = false;
		bool bIsHeld     = false;
		bool bIsReleased = false;
	};

	struct InputAxisState1D {
		float value = 0.0f;
	};

	enum class INPUT_AXIS : uint8_t {
		X, Y
	};

	struct InputAxisState2D {
		Vec2 value = Vec2::zero;
	};

	enum class BINDING_TYPE :uint8_t {
		ACTION,
		AXIS_1D,
		AXIS_2D
	};

	struct InputBinding {
		InputKey     key;
		BINDING_TYPE type;
		std::string  target;

		float      scale   = 1.0f;
		Vec2       scale2D = Vec2::one;
		INPUT_AXIS axis    = INPUT_AXIS::X;
	};

	class UInputSystem final : public ISubsystem, public IWin32MsgListener {
	public:
		~UInputSystem() override;

		// ISubsystem
		bool Init() override;
		void Update(float) override;

		[[nodiscard]] const std::string_view GetName() const override;

		// IWin32MsgListener
		bool OnWin32Message(HWND, UINT, WPARAM, LPARAM) override;

		//---------------------------------------------------------------------

		void RegisterDevice(const std::shared_ptr<BaseInputDevice>& device);

		void BindAction(const std::string& action, const InputKey& key);
		void BindAxis1D(
			const std::string& axis,
			const InputKey&    key,
			float              scale = 1.0f
		);
		void BindAxis2D(
			const std::string& axis,
			const InputKey&    key,
			const INPUT_AXIS&  axisType,
			const float&       scale = 1.0f
		);

		void HandleConsoleAction(
			const std::string& action, const bool& bPressed
		);

		[[nodiscard]] bool IsPressed(const std::string& action) const;
		[[nodiscard]] bool IsHeld(const std::string& action) const;
		[[nodiscard]] bool IsReleased(const std::string& action) const;

		[[nodiscard]] float Axis1D(const std::string& axis) const;
		[[nodiscard]] Vec2  Axis2D(const std::string& axis) const;

	private:
		void ResetInputStates();

		void OnRawInput(const RAWINPUT& rawInput);

		[[nodiscard]] bool GetHardwareKeyState(
			const InputKey& key
		) const;
		[[nodiscard]] float GetAnalogValue(
			const InputKey& key
		) const;

		static std::string GetKeyName(const UINT& virtualKey);

		std::unordered_map<std::string, InputActionState> mActionStates;
		std::unordered_map<std::string, InputAxisState1D> mAxisStates1D;
		std::unordered_map<std::string, InputAxisState2D> mAxisStates2D;

		std::vector<InputBinding>           mBindings;
		std::unordered_map<InputKey, bool>  mCurrentKeyStates;
		std::unordered_map<InputKey, bool>  mPreviousKeyStates;
		std::unordered_map<InputKey, float> mAnalogValues;

		std::vector<std::shared_ptr<BaseInputDevice>> mDevices;
	};
}
