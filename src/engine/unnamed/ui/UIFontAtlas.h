#pragma once

#include <array>
#include <cstdint>

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::UI {
	/// @brief 1文字分のグリフ情報です。
	struct UIGlyph {
		float advanceX = 0.0f;
		Vec2  size     = Vec2::zero;
		Vec2  bearing  = Vec2::zero;
		Vec2  uvMin    = Vec2::zero;
		Vec2  uvMax    = Vec2::zero;
		bool  hasBitmap = false;
	};

	/// @brief 最小UI向けフォントアトラスです（ASCII固定）。
	class UIFontAtlas {
	public:
		/// @brief フォントアトラス初期化を保証します。
		[[nodiscard]] bool EnsureInitialized(AssetManager& assetManager);

		/// @brief フォントアトラスが初期化済みか返します。
		[[nodiscard]] bool IsInitialized() const;

		/// @brief 生成済みフォントアトラスのテクスチャAssetIDを返します。
		[[nodiscard]] AssetID GetTextureAssetId() const;

		/// @brief フォントピクセルサイズを返します。
		[[nodiscard]] float GetFontPixelSize() const;

		/// @brief フォントピクセルサイズを設定します。
		/// @details サイズが変わった場合は次回 EnsureInitialized 時に再生成します。
		void SetFontPixelSize(float fontPixelSize);

		/// @brief stbtt packing の oversampling 設定を返します。
		[[nodiscard]] uint32_t GetOversampleH() const;
		[[nodiscard]] uint32_t GetOversampleV() const;

		/// @brief stbtt packing の oversampling 設定を更新します。
		/// @details 値が変わった場合は次回 EnsureInitialized 時に再生成します。
		void SetOversampling(uint32_t oversampleH, uint32_t oversampleV);

		/// @brief フォントのascent（px）を返します。
		[[nodiscard]] float GetAscentPx() const;

		/// @brief 文字コードに対応するグリフ情報を返します（ASCIIのみ）。
		[[nodiscard]] const UIGlyph* FindGlyph(char character) const;

	private:
		[[nodiscard]] bool BuildAtlas(AssetManager& assetManager);

	private:
		static constexpr uint32_t kAsciiFirst = 32;
		static constexpr uint32_t kAsciiLast  = 126;
		static constexpr size_t   kGlyphTableSize = kAsciiLast + 1;

		AssetID mTextureAssetId = kInvalidAssetID;
		float   mFontPixelSize  = 20.0f;
		uint32_t mOversampleH   = 1;
		uint32_t mOversampleV   = 1;
		float   mAscentPx       = 0.0f;
		bool    mInitialized    = false;
		std::array<UIGlyph, kGlyphTableSize> mGlyphs = {};
	};

	/// @brief UIフォントアトラスのシングルトンを返します。
	[[nodiscard]] UIFontAtlas& GetUIFontAtlas();
}
