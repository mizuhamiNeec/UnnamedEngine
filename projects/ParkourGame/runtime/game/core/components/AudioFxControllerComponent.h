#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class AudioSourceComponent;
	class JsonReader;
	class JsonWriter;

	class AudioFxControllerComponent final : public BaseComponent {
	public:
		struct OneShotPreset {
			std::string id;
			uint64_t    sourceEntityGuid    = 0;
			uint64_t    sourceComponentGuid = 0;
			float       volume              = 1.0f;
			float       pitchMin            = 1.0f;
			float       pitchMax            = 1.0f;
		};

		void OnAttached() override;
		void OnTick(float deltaTime) override;

		[[nodiscard]] bool TriggerOneShot(
			std::string_view presetId, float intensityScale = 1.0f
		) const;
		[[nodiscard]] bool HasPreset(std::string_view presetId) const;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		[[nodiscard]] const OneShotPreset* FindPreset(std::string_view id) const;
		[[nodiscard]] AudioSourceComponent* ResolveSource(
			const OneShotPreset& preset
		) const;

		std::vector<OneShotPreset> mOneShotPresets;
	};
}
