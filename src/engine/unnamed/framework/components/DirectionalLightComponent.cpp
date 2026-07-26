#include "DirectionalLightComponent.h"

#include <algorithm>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "TransformComponent.h"

#include "engine/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	void DirectionalLightComponent::SetColor(const Vec3& color) noexcept {
		mColor = Vec3(
			std::max(0.0f, color.x),
			std::max(0.0f, color.y),
			std::max(0.0f, color.z)
		);
	}

	const Vec3& DirectionalLightComponent::GetColor() const noexcept {
		return mColor;
	}

	void DirectionalLightComponent::SetIntensity(
		const float intensity
	) noexcept {
		mIntensity = std::max(0.0f, intensity);
	}

	float DirectionalLightComponent::GetIntensity() const noexcept {
		return mIntensity;
	}

	void DirectionalLightComponent::SetCastsShadow(
		const bool castsShadow
	) noexcept {
		mCastsShadow = castsShadow;
	}

	bool DirectionalLightComponent::GetCastsShadow() const noexcept {
		return mCastsShadow;
	}

	bool DirectionalLightComponent::BuildLightInput(
		Render::DirectionalLightInput& outLight
	) const {
		const Entity* owner = GetOwner();
		if (!owner || !IsActive() || mIntensity <= 0.0f) {
			return false;
		}

		const auto* transform = owner->GetComponent<TransformComponent>();
		Vec3        lightRayDirection = transform ?
			                         transform->Forward() :
			                         Vec3(0.0f, -1.0f, 0.0f);
		if (lightRayDirection.IsZero()) {
			lightRayDirection = Vec3(0.0f, -1.0f, 0.0f);
		}
		lightRayDirection = lightRayDirection.Normalized();

		outLight.enabled           = true;
		outLight.castsShadow       = mCastsShadow;
		outLight.lightRayDirection = lightRayDirection;
		outLight.directionToLight  = lightRayDirection * -1.0f;
		outLight.color             = mColor;
		outLight.intensity         = mIntensity;
		return true;
	}

	std::string_view DirectionalLightComponent::GetStableName() const {
		return "engine.DirectionalLight";
	}

	std::string_view DirectionalLightComponent::GetComponentName() const {
		return "DirectionalLight";
	}

	uint32_t DirectionalLightComponent::GetIcon() const {
		return kIconVisibility;
	}

	void DirectionalLightComponent::Deserialize(const JsonReader& reader) {
		SetColor(reader["color"].GetVec3(mColor));
		SetIntensity(reader.ReadFloatOr("intensity", mIntensity));
		SetCastsShadow(reader.ReadBoolOr("castsShadow", mCastsShadow));
	}

	void DirectionalLightComponent::Serialize(JsonWriter& writer) const {
		writer.WriteVec3("color", mColor);
		writer.Key("intensity");
		writer.Write(mIntensity);
		writer.Key("castsShadow");
		writer.Write(mCastsShadow);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void DirectionalLightComponent::DrawInspectorImGui() {
		float color[3] = {mColor.x, mColor.y, mColor.z};
		if (ImGui::ColorEdit3("Color", color)) {
			SetColor(Vec3(color[0], color[1], color[2]));
		}
		if (ImGui::DragFloat("Intensity", &mIntensity, 0.01f, 0.0f, 32.0f)) {
			SetIntensity(mIntensity);
		}
		ImGui::Checkbox("Casts Shadow", &mCastsShadow);
		ImGui::TextUnformatted("Direction uses Transform forward.");
	}
#endif

	REGISTER_COMPONENT(DirectionalLightComponent);
}
