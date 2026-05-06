#include "RotatorComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Quaternion.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	void RotatorComponent::OnTick(const float deltaTime) {
		if (!mRotationEnabled) {
			return;
		}
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		const Quaternion deltaRotation = Quaternion::EulerDegrees(
			mRotationRate * deltaTime
		);
		transform->SetRotation(transform->GetRotation() * deltaRotation);
	}

	BaseComponent::TICK_GROUP RotatorComponent::GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}

	void RotatorComponent::Deserialize(const JsonReader& reader) {
		const JsonReader rotationRate = reader["rotationRate"];
		if (rotationRate.Valid() && rotationRate.Size() >= 3) {
			mRotationRate = Vec3(
				rotationRate[0].GetFloat(),
				rotationRate[1].GetFloat(),
				rotationRate[2].GetFloat()
			);
		}
		const JsonReader enabled = reader["enabled"];
		if (enabled.Valid()) {
			mRotationEnabled = enabled.GetBool();
		}
	}

	void RotatorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("rotationRate");
		writer.BeginArray();
		writer.Write(mRotationRate.x);
		writer.Write(mRotationRate.y);
		writer.Write(mRotationRate.z);
		writer.EndArray();
		writer.Key("enabled");
		writer.Write(mRotationEnabled);
	}

	uint32_t RotatorComponent::GetIcon() const {
		return kIcon3DRotation;
	}

#ifdef _DEBUG
	void RotatorComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enabled", &mRotationEnabled);
		ImGui::DragFloat3("RotationRate", &mRotationRate.x, 0.1f);
	}
#endif

	TransformComponent* RotatorComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(RotatorComponent);
}
