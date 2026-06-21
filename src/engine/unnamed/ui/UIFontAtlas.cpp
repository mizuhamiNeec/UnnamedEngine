#include "UIFontAtlas.h"
#include "core/filesystem/Path.h"

#include "pch.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/ImGui/imstb_truetype.h" // TODO: 暫定でImGuiのstb_truetypeを使用; 将来的には独立させる


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/TextureAssetData.h"


#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::UI {
	namespace {
		constexpr std::string_view kChannel = "UI";

		struct PendingGlyph {
			stbtt_packedchar packed    = {};
			float            advanceX  = 0.0f;
			float            bearingX  = 0.0f;
			float            bearingY  = 0.0f;
			float            widthPx   = 0.0f;
			float            heightPx  = 0.0f;
			int              srcX0     = 0;
			int              srcY0     = 0;
			int              srcX1     = 0;
			int              srcY1     = 0;
			bool             hasBitmap = false;
		};

		[[nodiscard]] int GetTextDebugMode() {
			if (auto* console = ServiceLocator::Get<ConsoleSystem>();
				console != nullptr) {
				if (const auto* textDebugModeVar =
						console->GetConVarAs<ConVar<int>>(
							"ui_new_text_debug_mode"
						);
					textDebugModeVar != nullptr) {
					return textDebugModeVar->GetValue();
				}
			}
			return 0;
		}

		[[nodiscard]] bool ReadBinaryFile(
			const Path& path, std::vector<uint8_t>& outBytes
		) {
			std::ifstream input(
				path.Native(),
				std::ios::binary | std::ios::ate
			);
			if (!input.is_open()) {
				return false;
			}

			const std::streamsize byteSize = input.tellg();
			if (byteSize <= 0) {
				return false;
			}

			outBytes.resize(static_cast<size_t>(byteSize));
			input.seekg(0, std::ios::beg);
			return input.read(
				reinterpret_cast<char*>(outBytes.data()),
				byteSize
			).good();
		}

		[[nodiscard]] float KeyToFontSizePx(const UIFontAtlasKey& key) {
			return std::max(8.0f, static_cast<float>(key.fontSize100) / 100.0f);
		}
	}

	UIFontAtlasKey MakeUIFontAtlasKey(
		const Path&  fontPath,
		const float  fontSizePx,
		const uint32_t oversampleH,
		const uint32_t oversampleV
	) {
		UIFontAtlasKey key = {};
		key.fontPath       = fontPath.IsEmpty() ? Path() : fontPath.LexicallyNormal();
		key.fontSize100    = static_cast<int32_t>(std::lround(
			std::clamp(fontSizePx, 8.0f, 96.0f) * 100.0f
		));
		key.oversampleH = std::clamp(oversampleH, 1u, 8u);
		key.oversampleV = std::clamp(oversampleV, 1u, 8u);
		return key;
	}

	bool UIFontAtlas::EnsureInitialized(AssetManager& assetManager) {
		if (mInitialized && mTextureAssetId != kInvalidAssetID) {
			return true;
		}
		if (!BuildAtlas(assetManager)) {
			return false;
		}
		mInitialized = true;
		return true;
	}

	bool UIFontAtlas::IsInitialized() const {
		return mInitialized && mTextureAssetId != kInvalidAssetID;
	}

	AssetID UIFontAtlas::GetTextureAssetId() const {
		return mTextureAssetId;
	}

	float UIFontAtlas::GetFontPixelSize() const {
		return mFontPixelSize;
	}

	const Path& UIFontAtlas::GetFontPath() const {
		return mFontPath;
	}

	void UIFontAtlas::SetFontPixelSize(const float fontPixelSize) {
		const float clampedSize = std::clamp(fontPixelSize, 8.0f, 96.0f);
		if (std::fabs(clampedSize - mFontPixelSize) < 0.01f) {
			return;
		}
		mFontPixelSize = clampedSize;
		mAscentPx      = 0.0f;
		mDescentPx     = 0.0f;
		mLineGapPx     = 0.0f;
		mLineHeightPx  = clampedSize;
		mInitialized   = false;
	}

	void UIFontAtlas::SetFontPath(Path fontPath) {
		fontPath = fontPath.IsEmpty() ? Path() : fontPath.LexicallyNormal();
		if (fontPath.IsEmpty() || fontPath == mFontPath) {
			return;
		}
		mFontPath    = std::move(fontPath);
		mInitialized = false;
	}

	uint32_t UIFontAtlas::GetOversampleH() const {
		return mOversampleH;
	}

	uint32_t UIFontAtlas::GetOversampleV() const {
		return mOversampleV;
	}

	float UIFontAtlas::MeasureTextWidth(const std::string& text) const {
		float width = 0.0f;
		for (const char character : text) {
			const UIGlyph* glyph = FindGlyph(character);
			if (glyph == nullptr) {
				continue;
			}
			width += glyph->advanceX;
		}
		return width;
	}

	float UIFontAtlas::GetLineHeight() const {
		return std::max(1.0f, mLineHeightPx);
	}

	void UIFontAtlas::SetOversampling(
		const uint32_t oversampleH, const uint32_t oversampleV
	) {
		const uint32_t clampedH = std::clamp(oversampleH, 1u, 8u);
		const uint32_t clampedV = std::clamp(oversampleV, 1u, 8u);
		if (clampedH == mOversampleH && clampedV == mOversampleV) {
			return;
		}

		mOversampleH = clampedH;
		mOversampleV = clampedV;
		mInitialized = false;
	}

	float UIFontAtlas::GetAscentPx() const {
		return mAscentPx;
	}

	const UIGlyph* UIFontAtlas::FindGlyph(const char character) const {
		const uint8_t code = static_cast<uint8_t>(character);
		if (code >= mGlyphs.size()) {
			return nullptr;
		}
		return &mGlyphs[code];
	}

	bool UIFontAtlas::BuildAtlas(AssetManager& assetManager) {
		ReleaseTextureAsset(assetManager, "rebuild");

		constexpr int        kAsciiCount = kAsciiLast - kAsciiFirst + 1;
		std::vector<uint8_t> ttfBytes;
		if (!ReadBinaryFile(mFontPath, ttfBytes)) {
			Warning("UI", "UIFontAtlas failed to read font file: {}",
			        mFontPath);
			return false;
		}

		stbtt_fontinfo fontInfo = {};
		const int fontOffset = stbtt_GetFontOffsetForIndex(ttfBytes.data(), 0);
		if (
			fontOffset < 0 ||
			!stbtt_InitFont(&fontInfo, ttfBytes.data(), fontOffset)
		) {
			Warning("UI", "UIFontAtlas failed to init stb_truetype font.");
			return false;
		}

		const float scale = stbtt_ScaleForPixelHeight(
			&fontInfo, mFontPixelSize
		);
		int ascent  = 0;
		int descent = 0;
		int lineGap = 0;
		stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
		mAscentPx     = static_cast<float>(ascent) * scale;
		mDescentPx    = static_cast<float>(descent) * scale;
		mLineGapPx    = static_cast<float>(lineGap) * scale;
		mLineHeightPx = static_cast<float>(ascent - descent + lineGap) * scale;

		constexpr int        packAtlasWidth  = 2048;
		constexpr int        packAtlasHeight = 1024;
		std::vector<uint8_t> packedAtlas(
			static_cast<size_t>(packAtlasWidth) * static_cast<size_t>(
				packAtlasHeight),
			0
		);
		std::array<stbtt_packedchar, kAsciiCount> packedChars = {};
		stbtt_pack_context                        packContext = {};
		if (
			!stbtt_PackBegin(
				&packContext,
				packedAtlas.data(),
				packAtlasWidth,
				packAtlasHeight,
				0,
				2,
				nullptr
			)
		) {
			Warning("UI", "UIFontAtlas failed to begin glyph packing.");
			return false;
		}
		stbtt_PackSetOversampling(
			&packContext,
			static_cast<unsigned int>(mOversampleH),
			static_cast<unsigned int>(mOversampleV)
		);
		const int packed = stbtt_PackFontRange(
			&packContext,
			ttfBytes.data(),
			0,
			mFontPixelSize,
			static_cast<int>(kAsciiFirst),
			kAsciiCount,
			packedChars.data()
		);
		stbtt_PackEnd(&packContext);
		if (packed == 0) {
			Warning("UI", "UIFontAtlas failed to pack ASCII glyphs.");
			return false;
		}

		std::array<PendingGlyph, kGlyphTableSize> pending          = {};
		int                                       bitmapGlyphCount = 0;
		for (uint32_t code = kAsciiFirst; code <= kAsciiLast; ++code) {
			PendingGlyph& glyph       = pending[code];
			const size_t  packedIndex = static_cast<size_t>(code - kAsciiFirst);
			glyph.packed              = packedChars[packedIndex];
			glyph.advanceX            = glyph.packed.xadvance;
			glyph.bearingX            = glyph.packed.xoff;
			glyph.bearingY            = glyph.packed.yoff;
			glyph.widthPx             = std::max(
				0.0f, glyph.packed.xoff2 - glyph.packed.xoff
			);
			glyph.heightPx = std::max(
				0.0f, glyph.packed.yoff2 - glyph.packed.yoff
			);
			glyph.srcX0     = glyph.packed.x0;
			glyph.srcY0     = glyph.packed.y0;
			glyph.srcX1     = glyph.packed.x1;
			glyph.srcY1     = glyph.packed.y1;
			glyph.hasBitmap = (glyph.srcX1 > glyph.srcX0) &&
			                  (glyph.srcY1 > glyph.srcY0);
			if (glyph.hasBitmap) {
				++bitmapGlyphCount;
			}
		}

		std::vector<uint8_t> rgbaAtlas(
			static_cast<size_t>(packAtlasWidth) *
			static_cast<size_t>(packAtlasHeight) * 4,
			255
		);
		for (size_t i = 0; i < packedAtlas.size(); ++i) {
			rgbaAtlas[i * 4 + 3] = packedAtlas[i];
		}

		TextureAssetData texture = {};
		texture.width            = static_cast<uint32_t>(packAtlasWidth);
		texture.height           = static_cast<uint32_t>(packAtlasHeight);
		texture.arraySize        = 1;
		texture.mipLevels        = 1;
		texture.format           = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture.isSRGB           = false;
		texture.sourcePath       = Path(
			std::format(
				"runtime://ui/font_atlas_ascii/{}_{}x{}_{}",
				mFontPixelSize,
				mOversampleH,
				mOversampleV,
				bitmapGlyphCount
			)
		);

		TextureMip mip = {};
		mip.width      = texture.width;
		mip.height     = texture.height;
		mip.rowPitch   = static_cast<size_t>(texture.width) * 4;
		mip.bytes      = std::move(rgbaAtlas);
		texture.mips.emplace_back(std::move(mip));

		mTextureAssetId = assetManager.CreateRuntimeAsset<TextureAssetData>(
			ASSET_TYPE::TEXTURE,
			std::format(
				"runtime.ui.font_atlas_ascii.{}.{}x{}.{}",
				mFontPixelSize,
				mOversampleH,
				mOversampleV,
				bitmapGlyphCount
			),
			std::move(texture)
		);
		if (mTextureAssetId == kInvalidAssetID) {
			Warning(
				"UI", "UIFontAtlas failed to register runtime texture asset.");
			return false;
		}
		assetManager.AddRef(mTextureAssetId);

		for (UIGlyph& glyph : mGlyphs) {
			glyph = {};
		}
		constexpr float invWidth  = 1.0f / static_cast<float>(packAtlasWidth);
		constexpr float invHeight = 1.0f / static_cast<float>(packAtlasHeight);
		for (uint32_t code = kAsciiFirst; code <= kAsciiLast; ++code) {
			const PendingGlyph& src = pending[code];
			UIGlyph&            dst = mGlyphs[code];
			dst.advanceX            = src.advanceX;
			dst.bearing             = Vec2(src.bearingX, src.bearingY);
			dst.size                = Vec2(src.widthPx, src.heightPx);
			dst.hasBitmap           = src.hasBitmap;
			if (!src.hasBitmap) {
				continue;
			}
			dst.uvMin = Vec2(
				static_cast<float>(src.srcX0) * invWidth,
				static_cast<float>(src.srcY0) * invHeight
			);
			dst.uvMax = Vec2(
				static_cast<float>(src.srcX1) * invWidth,
				static_cast<float>(src.srcY1) * invHeight
			);
		}

		if (GetTextDebugMode() >= 1) {
			DevMsg(
				kChannel,
				"UIFontAtlas initialized: font='{}', size={}, oversample={}x{}, lineHeight={}, textureAssetId={}, bitmapGlyphs={}",
				mFontPath,
				mFontPixelSize,
				mOversampleH,
				mOversampleV,
				mLineHeightPx,
				mTextureAssetId,
				bitmapGlyphCount
			);
		}
		return true;
	}

	bool UIFontAtlas::ReleaseTextureAsset(
		AssetManager& assetManager, const std::string_view reason
	) {
		const AssetID oldTextureAssetId = mTextureAssetId;
		if (oldTextureAssetId == kInvalidAssetID) {
			return false;
		}

		const bool isRuntimeAsset = assetManager.IsRuntimeAsset(
			oldTextureAssetId
		);
		mTextureAssetId = kInvalidAssetID;
		mInitialized    = false;

		assetManager.Release(oldTextureAssetId);
		if (!isRuntimeAsset) {
			Warning(
				kChannel,
				"UIFontAtlas skipped DestroyRuntimeAsset for non-runtime texture asset: assetId={}, reason={}",
				oldTextureAssetId,
				reason
			);
			return false;
		}

		if (!assetManager.DestroyRuntimeAsset(oldTextureAssetId)) {
			Warning(
				kChannel,
				"UIFontAtlas failed to destroy runtime texture asset: assetId={}, reason={}",
				oldTextureAssetId,
				reason
			);
			return false;
		}

		if (GetTextDebugMode() >= 1) {
			DevMsg(
				kChannel,
				"UIFontAtlas destroyed runtime texture asset: assetId={}, reason={}",
				oldTextureAssetId,
				reason
			);
		}
		return true;
	}

	UIFontAtlas* UIFontAtlasCache::GetOrCreate(
		const UIFontAtlasKey& key, AssetManager& assetManager
	) {
		++mUseCounter;
		for (CacheEntry& entry : mEntries) {
			if (!(entry.key == key)) {
				continue;
			}
			entry.lastUsedFrame    = mUseCounter;
			mCurrentKey            = key;
			mCurrentTextureAssetId = entry.atlas ?
				                         entry.atlas->GetTextureAssetId() :
				                         kInvalidAssetID;
			return entry.atlas.get();
		}

		auto atlas = std::make_unique<UIFontAtlas>();
		atlas->SetFontPath(key.fontPath);
		atlas->SetFontPixelSize(KeyToFontSizePx(key));
		atlas->SetOversampling(key.oversampleH, key.oversampleV);
		if (!atlas->EnsureInitialized(assetManager)) {
			return nullptr;
		}

		++mCreateRuntimeAssetCallCount;
		mCurrentKey            = key;
		mCurrentTextureAssetId = atlas->GetTextureAssetId();

		CacheEntry entry    = {};
		entry.key           = key;
		entry.atlas         = std::move(atlas);
		entry.lastUsedFrame = mUseCounter;
		mEntries.emplace_back(std::move(entry));
		PruneIfNeeded(assetManager);
		return mEntries.back().atlas.get();
	}

	void UIFontAtlasCache::Clear(AssetManager* assetManager) {
		if (assetManager != nullptr) {
			for (const CacheEntry& entry : mEntries) {
				if (
					entry.atlas &&
					entry.atlas->GetTextureAssetId() != kInvalidAssetID
				) {
					if (entry.atlas->ReleaseTextureAsset(
						*assetManager, "cache-clear"
					)) {
						++mDestroyRuntimeAssetCallCount;
					} else {
						++mDestroyRuntimeAssetFailedCount;
					}
				}
			}
		} else if (!mEntries.empty()) {
			Warning(
				kChannel,
				"UIFontAtlasCache::Clear called without AssetManager; runtime texture assets cannot be destroyed."
			);
			return;
		}
		mEntries.clear();
		mCurrentKey            = {};
		mCurrentTextureAssetId = kInvalidAssetID;
	}

	UIFontAtlasCacheDebugInfo UIFontAtlasCache::GetDebugInfo() const {
		UIFontAtlasCacheDebugInfo info      = {};
		info.cacheCount                     = mEntries.size();
		info.maxCacheEntries                = mMaxCacheEntries;
		info.createRuntimeAssetCallCount    = mCreateRuntimeAssetCallCount;
		info.destroyRuntimeAssetCallCount   = mDestroyRuntimeAssetCallCount;
		info.destroyRuntimeAssetFailedCount =
			mDestroyRuntimeAssetFailedCount;
		info.currentKey            = mCurrentKey;
		info.currentTextureAssetId = mCurrentTextureAssetId;
		return info;
	}

	void UIFontAtlasCache::PruneIfNeeded(AssetManager& assetManager) {
		while (mEntries.size() > mMaxCacheEntries) {
			auto it = std::ranges::min_element(
				mEntries,
				[](const CacheEntry& lhs, const CacheEntry& rhs) {
					return lhs.lastUsedFrame < rhs.lastUsedFrame;
				}
			);
			
			if (it == mEntries.end()) {
				break;
			}

			if (it->atlas && it->atlas->GetTextureAssetId() !=
			    kInvalidAssetID) {
				if (it->atlas->
				        ReleaseTextureAsset(assetManager, "cache-prune")) {
					++mDestroyRuntimeAssetCallCount;
				} else {
					++mDestroyRuntimeAssetFailedCount;
				}
			}
			mEntries.erase(it);
		}
	}

	UIFontAtlasCache& GetUIFontAtlasCache() {
		static UIFontAtlasCache sCache = {};
		return sCache;
	}
}
