#include "SkyLightComponent.h"

#include <algorithm>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] Vec3 ClampColor(const Vec3& color) noexcept {
			return Vec3(
				std::max(0.0f, color.x),
				std::max(0.0f, color.y),
				std::max(0.0f, color.z)
			);
		}

		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}
	}

	void SkyLightComponent::SetSkyColor(const Vec3& color) noexcept {
		mSkyColor = ClampColor(color);
	}

	const Vec3& SkyLightComponent::GetSkyColor() const noexcept {
		return mSkyColor;
	}

	void SkyLightComponent::SetGroundColor(const Vec3& color) noexcept {
		mGroundColor = ClampColor(color);
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
		SetIntensity(ReadFloatOr(reader, "intensity", mIntensity));
	}

	void SkyLightComponent::Serialize(JsonWriter& writer) const {
		writer.WriteVec3("skyColor", mSkyColor);
		writer.WriteVec3("groundColor", mGroundColor);
		writer.Key("intensity");
		writer.Write(mIntensity);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkyLightComponent::DrawInspectorImGui() {
		float skyColor[3] = {mSkyColor.x, mSkyColor.y, mSkyColor.z};
		if (ImGui::ColorEdit3("Sky Color", skyColor)) {
			SetSkyColor(Vec3(skyColor[0], skyColor[1], skyColor[2]));
		}

		float groundColor[3] = {mGroundColor.x, mGroundColor.y, mGroundColor.z};
		if (ImGui::ColorEdit3("Ground Color", groundColor)) {
			SetGroundColor(Vec3(groundColor[0], groundColor[1], groundColor[2]));
		}

		if (ImGui::DragFloat("Intensity", &mIntensity, 0.01f, 0.0f, 8.0f)) {
			SetIntensity(mIntensity);
		}
		ImGui::TextUnformatted("Currently feeds Hemisphere Ambient.");
	}
#endif

	REGISTER_COMPONENT(SkyLightComponent);
}
