#pragma once

#include "TriggerVolumeComponentBase.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	/// @brief SpeedBoostAreaComponentは、領域内characterへ設定倍率の移動速度補正を適用します
	class SpeedBoostAreaComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.SpeedBoostArea";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "SpeedBoostArea";
		}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] float GetMultiplier() const noexcept {
			return mMultiplier;
		}

		[[nodiscard]] float GetDurationSec() const noexcept {
			return mDurationSec;
		}

	private:
		float mMultiplier  = 1.5f;
		float mDurationSec = 3.0f;
	};
}

