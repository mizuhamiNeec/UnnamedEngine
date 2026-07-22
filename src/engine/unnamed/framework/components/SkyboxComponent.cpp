#include "SkyboxComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/TextureAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kDefaultSkyboxTexturePath =
			"textures/wave.dds";
	}

	bool SkyboxComponent::SetTexturePath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (
			mTexturePath.has_value() && *mTexturePath == path &&
			mTextureAssetId != kInvalidAssetID
		) {
			return true;
		}

		const AssetID assetId = assetManager.LoadTexture(path);
		if (assetId == kInvalidAssetID) {
			ClearTexturePath();
			return false;
		}

		const TextureAssetData* texture = assetManager.Get<TextureAssetData>(
			assetId
		);
		if (!texture || !texture->isCubeMap) {
			Error(
				"Skybox",
				"Skybox texture is not a cubemap: virtualPath={} assetId={}",
				path.String(),
				assetId
			);
			ClearTexturePath();
			return false;
		}

		mTexturePath    = path;
		mTextureAssetId = assetId;
		return true;
	}

	void SkyboxComponent::ClearTexturePath() noexcept {
		mTexturePath.reset();
		mTextureAssetId = kInvalidAssetID;
	}

	const std::optional<VirtualPath>& SkyboxComponent::GetTexturePath()
	const noexcept {
		return mTexturePath;
	}

	void SkyboxComponent::SetIntensity(const float intensity) noexcept {
		mIntensity = std::max(0.0f, intensity);
	}

	float SkyboxComponent::GetIntensity() const noexcept {
		return mIntensity;
	}

	AssetID SkyboxComponent::GetTextureAssetId() const noexcept {
		return mTextureAssetId;
	}

	void SkyboxComponent::OnAttached() {
		if (
			mTextureAssetId != kInvalidAssetID || !mTexturePath.has_value()
		) {
			return;
		}
		if (AssetManager* assetManager = GetAssetManager()) {
			(void)SetTexturePath(*mTexturePath, *assetManager);
		}
	}

	std::string_view SkyboxComponent::GetStableName() const {
		return "engine.Skybox";
	}

	std::string_view SkyboxComponent::GetComponentName() const {
		return "Skybox";
	}

	void SkyboxComponent::Deserialize(const JsonReader& reader) {
		ClearTexturePath();
		const JsonReader value = reader["texturePath"];
		const std::string texturePath = value.Valid() && value.IsString() ?
			value.GetString() :
			std::string(kDefaultSkyboxTexturePath);
		mTexturePath = VirtualPath::ParseContentReference(texturePath);
		SetIntensity(reader.ReadFloatOr("intensity", mIntensity));
	}

	bool SkyboxComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		ClearTexturePath();
		SetIntensity(reader.ReadFloatOr("intensity", mIntensity));

		const JsonReader pathNode = reader["texturePath"];
		const std::string texturePath = pathNode.Valid() ?
			pathNode.GetString() :
			std::string(kDefaultSkyboxTexturePath);
		if (pathNode.Valid() && !pathNode.IsString()) {
			Error(
				"Skybox",
				"Skybox texturePath must be a string: scene='{}' entity='{}'",
				context.scenePath,
				context.entityName
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(texturePath);
		if (!virtualPath.has_value()) {
			Error(
				"Skybox",
				"Invalid Skybox texture virtual path: scene='{}' entity='{}' field='texturePath' value='{}'",
				context.scenePath,
				context.entityName,
				texturePath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (!context.assetManager) {
			Error(
				"Skybox",
				"AssetManager is unavailable while loading Skybox texture: {}",
				virtualPath->String()
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (SetTexturePath(*virtualPath, *context.assetManager)) {
			return true;
		}
		return !IsStrictAssetValidation(context.loadOptions);
	}

	void SkyboxComponent::Serialize(JsonWriter& writer) const {
		if (mTexturePath.has_value()) {
			writer.Key("texturePath");
			writer.Write(mTexturePath->String());
		}
		writer.Key("intensity");
		writer.Write(mIntensity);
	}

	uint32_t SkyboxComponent::GetIcon() const {
		return kIconPanoramaHorizontal;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkyboxComponent::DrawInspectorImGui() {
		std::string texturePath = mTexturePath.has_value() ?
			mTexturePath->String() :
			std::string{};
		if (
			ImGuiWidgets::AssetPathPicker(
				"TexturePath",
				texturePath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::TEXTURE)
			)
		) {
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(texturePath);
			AssetManager* assetManager = GetAssetManager();
			if (!virtualPath.has_value() || !assetManager) {
				ClearTexturePath();
			} else {
				(void)SetTexturePath(*virtualPath, *assetManager);
			}
		}

		if (ImGui::DragFloat("Intensity", &mIntensity, 0.01f, 0.0f, 32.0f)) {
			mIntensity = std::max(0.0f, mIntensity);
		}
	}
#endif

	REGISTER_COMPONENT(SkyboxComponent);
}
