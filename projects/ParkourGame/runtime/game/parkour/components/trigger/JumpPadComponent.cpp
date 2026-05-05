#include "JumpPadComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	void JumpPadComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader velocity = reader["boostVelocityHu"];
		if (velocity.Valid()) {
			mBoostVelocityHu = velocity.GetFloat();
		}
	}

	void JumpPadComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("boostVelocityHu");
		writer.Write(mBoostVelocityHu);
	}

#ifdef _DEBUG
	void JumpPadComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::DragFloat(
			"Boost Velocity HU",
			&mBoostVelocityHu,
			1.0f,
			0.0f,
			100000.0f
		);
	}
#endif

	REGISTER_COMPONENT(JumpPadComponent);
}
