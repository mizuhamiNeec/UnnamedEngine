#pragma once
#include <string>

#include "base/BaseComponent.h"

#include "core/assets/AssetID.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;

	class SkyboxComponent final : public BaseComponent {
	public:
		void SetTexturePath(const std::string& path);
		[[nodiscard]] const std::string& GetTexturePath() const noexcept;

		void                SetIntensity(float intensity) noexcept;
		[[nodiscard]] float GetIntensity() const noexcept;

		AssetID ResolveTextureAsset(AssetManager& assetManager);
		[[nodiscard]] AssetID GetTextureAssetId() const noexcept;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		std::string mTexturePath = "content/core/textures/wave.dds";
		AssetID     mTextureAssetId = kInvalidAssetID;
		float       mIntensity      = 1.0f;
	};
}
