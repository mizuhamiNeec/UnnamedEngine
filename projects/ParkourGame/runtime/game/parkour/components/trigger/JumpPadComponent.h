#pragma once

#include "TriggerVolumeComponentBase.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	/// @brief JumpPadComponentは、進入したcharacterへ設定方向・強度の跳躍速度を適用します
	class JumpPadComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.JumpPad";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "JumpPad";
		}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
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

