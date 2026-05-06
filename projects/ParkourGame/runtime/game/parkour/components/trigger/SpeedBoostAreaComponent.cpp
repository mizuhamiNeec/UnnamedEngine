#include "SpeedBoostAreaComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	void SpeedBoostAreaComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader multiplier = reader["multiplier"];
		const JsonReader duration   = reader["durationSec"];
		if (multiplier.Valid()) {
			mMultiplier = multiplier.GetFloat();
		}
		if (duration.Valid()) {
			mDurationSec = duration.GetFloat();
		}
	}

	void SpeedBoostAreaComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("multiplier");
		writer.Write(mMultiplier);
		writer.Key("durationSec");
		writer.Write(mDurationSec);
	}

#ifdef _DEBUG
	void SpeedBoostAreaComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::DragFloat("Multiplier", &mMultiplier, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("Duration Sec", &mDurationSec, 0.01f, 0.0f, 3600.0f);
	}
#endif

	REGISTER_COMPONENT(SpeedBoostAreaComponent);
}
