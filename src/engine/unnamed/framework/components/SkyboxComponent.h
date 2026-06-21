#pragma once
#include <string>

#include "base/BaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;

	class SkyboxComponent final : public BaseComponent {
	public:
		void                      SetTexturePath(Path path);
		[[nodiscard]] const Path& GetTexturePath() const noexcept;

		void                SetIntensity(float intensity) noexcept;
		[[nodiscard]] float GetIntensity() const noexcept;

		AssetID               ResolveTextureAsset(AssetManager& assetManager);
		[[nodiscard]] AssetID GetTextureAssetId() const noexcept;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		Path    mTexturePath    = Path("content/core/textures/wave.dds");
		AssetID mTextureAssetId = kInvalidAssetID;
		float   mIntensity      = 1.0f;
	};
}
