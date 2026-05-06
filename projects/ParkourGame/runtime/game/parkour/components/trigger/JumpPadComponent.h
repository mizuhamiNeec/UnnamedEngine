#pragma once

#include "TriggerVolumeComponentBase.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	class JumpPadComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.JumpPad";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "JumpPad";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] float GetBoostVelocityHu() const noexcept {
			return mBoostVelocityHu;
		}

	private:
		float mBoostVelocityHu = 800.0f;
	};
}
