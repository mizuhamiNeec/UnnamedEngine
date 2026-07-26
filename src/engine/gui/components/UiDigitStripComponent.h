#pragma once

#include <optional>

#include "UiComponent.h"

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	/// @brief UiDigitStripComponentは、数値を桁ごとのtexture領域へ変換して連続描画します
	class UiDigitStripComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "DigitStrip";
		}

		/// @brief 桁テクスチャ論理パスを解決して設定します。
		[[nodiscard]] bool SetStripTexturePath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief content root基準文字列を検証して桁テクスチャを設定します。
		[[nodiscard]] bool SetStripTexturePath(
			std::string_view path, AssetManager& assetManager
		);
		/// @brief 桁テクスチャ参照を未設定に戻します。
		void ClearStripTexturePath() noexcept;
		[[nodiscard]] const std::optional<VirtualPath>& GetStripTexturePath()
		const noexcept;
		[[nodiscard]] AssetID GetStripTextureAssetId() const noexcept;

		void              SetValue(int value);
		[[nodiscard]] int GetValue() const;

		void              SetMinDigits(int minDigits);
		[[nodiscard]] int GetMinDigits() const;

		void                SetDigitSpacing(float spacingPx);
		[[nodiscard]] float GetDigitSpacing() const;

		void                       SetColor(const Color& color);
		[[nodiscard]] const Color& GetColor() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;

		void Serialize(JsonWriter& writer) const override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		) override;

	private:
		std::optional<VirtualPath> mStripTexturePath;
		AssetID mStripTextureAssetId = kInvalidAssetID;
		int   mValue        = 0;
		int   mMinDigits    = 2;
		float mDigitSpacing = 0.0f;
		Color mColor        = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
	};
}
