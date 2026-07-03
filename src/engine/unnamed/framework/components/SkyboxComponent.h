#pragma once

#include <optional>
#include <string>

#include "base/BaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;

	class SkyboxComponent final : public BaseComponent {
	public:
		/// @brief Skybox cubemapを論理パスからロードして設定します。
		[[nodiscard]] bool SetTexturePath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief Skyboxテクスチャ参照を未設定に戻します。
		void ClearTexturePath() noexcept;
		[[nodiscard]] const std::optional<VirtualPath>& GetTexturePath()
		const noexcept;

		void                SetIntensity(float intensity) noexcept;
		[[nodiscard]] float GetIntensity() const noexcept;

		[[nodiscard]] AssetID GetTextureAssetId() const noexcept;
		void OnAttached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		std::optional<VirtualPath> mTexturePath =
			VirtualPath::ParseContentReference("textures/wave.dds");
		AssetID mTextureAssetId = kInvalidAssetID;
		float   mIntensity      = 1.0f;
	};
}
