#include "SkyLightComponent.h"

#include <algorithm>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "engine/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	void SkyLightComponent::SetSkyColor(const Vec3& color) noexcept {
		mSkyColor = color;
	}

	const Vec3& SkyLightComponent::GetSkyColor() const noexcept {
		return mSkyColor;
	}

	void SkyLightComponent::SetGroundColor(const Vec3& color) noexcept {
		mGroundColor = color;
	}

	const Vec3& SkyLightComponent::GetGroundColor() const noexcept {
		return mGroundColor;
	}

	void SkyLightComponent::SetIntensity(const float intensity) noexcept {
		mIntensity = std::max(0.0f, intensity);
	}

	float SkyLightComponent::GetIntensity() const noexcept {
		return mIntensity;
	}

	bool SkyLightComponent::BuildLightInput(
		Render::EnvironmentLightInput& outLight
	) const {
		if (!IsActive()) {
			return false;
		}

		outLight.enabled     = mIntensity > 0.0f;
		outLight.skyColor    = mSkyColor;
		outLight.groundColor = mGroundColor;
		outLight.intensity   = mIntensity;
		return true;
	}

	std::string_view SkyLightComponent::GetStableName() const {
		return "engine.SkyLight";
	}

	std::string_view SkyLightComponent::GetComponentName() const {
		return "SkyLight";
	}

	uint32_t SkyLightComponent::GetIcon() const {
		return kIconPanoramaHorizontal;
	}

	void SkyLightComponent::Deserialize(const JsonReader& reader) {
		SetSkyColor(reader["skyColor"].GetVec3(mSkyColor));
		SetGroundColor(reader["groundColor"].GetVec3(mGroundColor));
		SetIntensity(reader.ReadFloatOr("intensity", mIntensity));
	}

	void SkyLightComponent::Serialize(JsonWriter& writer) const {
		writer.WriteVec3("skyColor", mSkyColor);
		writer.WriteVec3("groundColor", mGroundColor);
		writer.Key("intensity");
		writer.Write(mIntensity);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkyLightComponent::DrawInspectorImGui() {
		Vec3 tmpSkyColor = mSkyColor;
		if (ImGui::ColorEdit3("Sky Color", &tmpSkyColor.x)) {
			SetSkyColor(tmpSkyColor);
		}

		Vec3 tmpGroundColor = mGroundColor;
		if (ImGui::ColorEdit3("Ground Color", &tmpGroundColor.x)) {
			SetGroundColor(tmpGroundColor);
		}

		if (ImGui::DragFloat("Intensity", &mIntensity, 0.01f, 0.0f, 8.0f)) {
			SetIntensity(mIntensity);
		}
		ImGui::TextUnformatted("現状 ヘミスフィア にのみ対応しています。");
	}
#endif

	REGISTER_COMPONENT(SkyLightComponent);
}
