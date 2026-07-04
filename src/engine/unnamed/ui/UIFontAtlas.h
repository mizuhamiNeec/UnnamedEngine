#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"
#include "core/math/Vec2.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::UI {
	/// @brief UIフォントアトラスを一意に識別するキーです。
	struct UIFontAtlasKey {
		VirtualPath fontPath = {};
		int32_t  fontSize100 = 2000;
		uint32_t oversampleH = 1;
		uint32_t oversampleV = 1;

		[[nodiscard]] bool operator==(const UIFontAtlasKey& rhs) const = default
		;
	};

	/// @brief フォントサイズとoversampleからUIFontAtlasKeyを生成します。
	[[nodiscard]] UIFontAtlasKey MakeUIFontAtlasKey(
		const VirtualPath& fontPath,
		float       fontSizePx,
		uint32_t    oversampleH,
		uint32_t    oversampleV
	);

	/// @brief 1文字分のグリフ情報です。
	struct UIGlyph {
		float advanceX  = 0.0f;
		Vec2  size      = Vec2::zero;
		Vec2  bearing   = Vec2::zero;
		Vec2  uvMin     = Vec2::zero;
		Vec2  uvMax     = Vec2::zero;
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
		[[nodiscard]] float       GetFontPixelSize() const;
		[[nodiscard]] const Path& GetFontPath() const;

		/// @brief フォントピクセルサイズを設定します。
		/// @details サイズが変わった場合は次回 EnsureInitialized 時に再生成します。
		void SetFontPixelSize(float fontPixelSize);
		void SetFontPath(Path fontPath);

		/// @brief stbtt packing の oversampling 設定を返します。
		[[nodiscard]] uint32_t GetOversampleH() const;
		[[nodiscard]] uint32_t GetOversampleV() const;

		/// @brief テキストの幅をピクセル単位で測定します。
		/// @details 対応文字はASCIIのみで、非対応文字は幅0として扱います。
		/// @param text 測定するテキスト
		/// @return テキストの幅（ピクセル単位）
		[[nodiscard]] float MeasureTextWidth(const std::string& text) const;

		/// @brief 行の高さをピクセル単位で返します。
		/// @details フォントのascent、descent、line gapを考慮して計算します。
		/// @return 行の高さ（ピクセル単位）
		[[nodiscard]] float GetLineHeight() const;

		/// @brief stbtt packing の oversampling 設定を更新します。
		/// @details 値が変わった場合は次回 EnsureInitialized 時に再生成します。
		void SetOversampling(uint32_t oversampleH, uint32_t oversampleV);

		/// @brief フォントのascent（px）を返します。
		[[nodiscard]] float GetAscentPx() const;

		/// @brief 文字コードに対応するグリフ情報を返します（ASCIIのみ）。
		[[nodiscard]] const UIGlyph* FindGlyph(char character) const;

	private:
		friend class UIFontAtlasCache;

		[[nodiscard]] bool BuildAtlas(AssetManager& assetManager);
		/// @brief 所有しているruntime texture assetをReleaseして明示破棄します。
		/// @param assetManager アセットマネージャー
		/// @param reason 破棄理由のログ用文字列
		/// @return DestroyRuntimeAsset が成功した場合true
		bool ReleaseTextureAsset(
			AssetManager& assetManager, std::string_view reason
		);

		static constexpr uint32_t kAsciiFirst     = 32;
		static constexpr uint32_t kAsciiLast      = 126;
		static constexpr size_t   kGlyphTableSize = kAsciiLast + 1;

		AssetID  mTextureAssetId = kInvalidAssetID;
		float    mFontPixelSize  = 20.0f;
		uint32_t mOversampleH    = 1;
		uint32_t mOversampleV    = 1;
		float    mAscentPx       = 0.0f;
		float    mDescentPx      = 0.0f;
		float    mLineGapPx      = 0.0f;
		float    mLineHeightPx   = 20.0f;
		Path     mFontPath       = {};
		bool                                 mInitialized = false;
		std::array<UIGlyph, kGlyphTableSize> mGlyphs      = {};
	};

	/// @brief UIFontAtlasCache のデバッグ情報です。
	struct UIFontAtlasCacheDebugInfo {
		size_t         cacheCount                     = 0;
		size_t         maxCacheEntries                = 0;
		uint64_t       createRuntimeAssetCallCount    = 0;
		uint64_t       destroyRuntimeAssetCallCount   = 0;
		uint64_t       destroyRuntimeAssetFailedCount = 0;
		UIFontAtlasKey currentKey                     = {};
		AssetID        currentTextureAssetId          = kInvalidAssetID;
	};

	/// @brief UIFontAtlas を設定キー単位で再利用する最小キャッシュです。
	class UIFontAtlasCache {
	public:
		/// @brief 同一キーなら既存Atlas、未作成なら新規生成して返します。
		[[nodiscard]] UIFontAtlas* GetOrCreate(
			const UIFontAtlasKey& key,
			const Path&           resolvedFontPath,
			AssetManager&         assetManager
		);

		/// @brief キャッシュ内容をクリアし、保持しているruntime texture assetを破棄します。
		void Clear(AssetManager* assetManager = nullptr);

		/// @brief 現在のキャッシュ状態を返します。
		[[nodiscard]] UIFontAtlasCacheDebugInfo GetDebugInfo() const;

	private:
		struct CacheEntry {
			UIFontAtlasKey               key = {};
			std::unique_ptr<UIFontAtlas> atlas;
			uint64_t                     lastUsedFrame = 0;
		};

		void PruneIfNeeded(AssetManager& assetManager);

		std::vector<CacheEntry> mEntries;
		size_t                  mMaxCacheEntries = 8;
		uint64_t                mUseCounter = 0;
		uint64_t                mCreateRuntimeAssetCallCount = 0;
		uint64_t                mDestroyRuntimeAssetCallCount = 0;
		uint64_t                mDestroyRuntimeAssetFailedCount = 0;
		UIFontAtlasKey          mCurrentKey = {};
		AssetID                 mCurrentTextureAssetId = kInvalidAssetID;
	};

	/// @brief UIフォントアトラスキャッシュのシングルトンを返します。
	[[nodiscard]] UIFontAtlasCache& GetUIFontAtlasCache();
}
