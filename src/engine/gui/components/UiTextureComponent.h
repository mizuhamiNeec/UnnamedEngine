#pragma once

#include <optional>

#include "UiComponent.h"

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	/// @brief UiTextureComponentは、asset textureとtintをUiWidgetのimage描画へ提供します
	class UiTextureComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "Texture";
		}

		/// @brief テクスチャ論理パスを解決して設定します。
		[[nodiscard]] bool SetTexturePath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief content root基準文字列を検証してテクスチャを設定します。
		[[nodiscard]] bool SetTexturePath(
			std::string_view path, AssetManager& assetManager
		);
		/// @brief テクスチャ参照を未設定に戻します。
		void ClearTexturePath() noexcept;
		[[nodiscard]] const std::optional<VirtualPath>& GetTexturePath()
		const noexcept;
		[[nodiscard]] AssetID GetTextureAssetId() const noexcept;

		void                       SetColor(const Color& color);
		[[nodiscard]] const Color& GetColor() const;

		void                      SetUvMin(const Vec2& uvMin);
		[[nodiscard]] const Vec2& GetUvMin() const;

		void                      SetUvMax(const Vec2& uvMax);
		[[nodiscard]] const Vec2& GetUvMax() const;

		void                      SetAnchor(const Vec2& anchor);
		[[nodiscard]] const Vec2& GetAnchor() const;

		void                SetRotationRad(float rotationRad);
		[[nodiscard]] float GetRotationRad() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;

		void Serialize(JsonWriter& writer) const override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		) override;

	private:
		std::optional<VirtualPath> mTexturePath;
		AssetID mTextureAssetId = kInvalidAssetID;
		Color mColor       = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
		Vec2  mUvMin       = Vec2(0.0f, 0.0f);
		Vec2  mUvMax       = Vec2(1.0f, 1.0f);
		Vec2  mAnchor      = Vec2(0.0f, 0.0f);
		float mRotationRad = 0.0f;
	};
}
