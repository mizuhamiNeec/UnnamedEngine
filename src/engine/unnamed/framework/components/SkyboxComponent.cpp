#include "SkyboxComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kDefaultSkyboxTexturePath =
			"content/core/textures/wave.dds";

		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}
	}

	void SkyboxComponent::SetTexturePath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mTexturePath == path) {
			return;
		}

		mTexturePath    = std::move(path);
		mTextureAssetId = kInvalidAssetID;
	}

	const Path& SkyboxComponent::GetTexturePath() const noexcept {
		return mTexturePath;
	}

	void SkyboxComponent::SetIntensity(const float intensity) noexcept {
		mIntensity = std::max(0.0f, intensity);
	}

	float SkyboxComponent::GetIntensity() const noexcept {
		return mIntensity;
	}

	AssetID SkyboxComponent::ResolveTextureAsset(AssetManager& assetManager) {
		if (mTexturePath.IsEmpty()) {
			return kInvalidAssetID;
		}
		if (mTextureAssetId != kInvalidAssetID) {
			return mTextureAssetId;
		}

		mTextureAssetId = assetManager.LoadFromFile(
			mTexturePath, ASSET_TYPE::TEXTURE
		);
		return mTextureAssetId;
	}

	AssetID SkyboxComponent::GetTextureAssetId() const noexcept {
		return mTextureAssetId;
	}

	std::string_view SkyboxComponent::GetStableName() const {
		return "engine.Skybox";
	}

	std::string_view SkyboxComponent::GetComponentName() const {
		return "Skybox";
	}

	void SkyboxComponent::Deserialize(const JsonReader& reader) {
		std::string texturePath = mTexturePath.ToGenericUtf8();
		if (const JsonReader value = reader["texturePath"];
			value.Valid()) {
			texturePath = value.GetString();
		}
		if (texturePath.empty()) {
			texturePath = std::string(kDefaultSkyboxTexturePath);
		}
		SetTexturePath(Path(texturePath));
		SetIntensity(ReadFloatOr(reader, "intensity", mIntensity));
	}

	void SkyboxComponent::Serialize(JsonWriter& writer) const {
		writer.Key("texturePath");
		writer.Write(mTexturePath.ToGenericUtf8());
		writer.Key("intensity");
		writer.Write(mIntensity);
	}

	uint32_t SkyboxComponent::GetIcon() const {
		return kIconPanoramaHorizontal;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkyboxComponent::DrawInspectorImGui() {
		std::string texturePath = mTexturePath.ToGenericUtf8();
		if (
			ImGuiWidgets::AssetPathPicker(
				"TexturePath",
				texturePath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::TEXTURE)
			)
		) {
			SetTexturePath(mTexturePath);
		}

		if (ImGui::DragFloat("Intensity", &mIntensity, 0.01f, 0.0f, 32.0f)) {
			mIntensity = std::max(0.0f, mIntensity);
		}
	}
#endif

	REGISTER_COMPONENT(SkyboxComponent);
}
