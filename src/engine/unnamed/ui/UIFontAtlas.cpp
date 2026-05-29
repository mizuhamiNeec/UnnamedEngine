#include "UIFontAtlas.h"

#include "pch.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/ImGui/imstb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/TextureAssetData.h"
#include "core/path/PathUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::UI {
	namespace {
		constexpr std::string_view kChannel = "UI";

		struct PendingGlyph {
			int              codepoint = 0;
			stbtt_packedchar packed    = {};
			float            advanceX  = 0.0f;
			float            bearingX  = 0.0f;
			float            bearingY  = 0.0f;
			float            widthPx   = 0.0f;
			float            heightPx  = 0.0f;
			int              width     = 0;
			int              height    = 0;
			int              srcX0     = 0;
			int              srcY0     = 0;
			int              srcX1     = 0;
			int              srcY1     = 0;
			bool hasBitmap = false;
		};

		[[nodiscard]] bool ReadBinaryFile(
			const std::string& path, std::vector<uint8_t>& outBytes
		) {
			std::ifstream input(
				Path::FromUtf8(path),
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

	void UIFontAtlas::SetFontPixelSize(const float fontPixelSize) {
		const float clampedSize = std::clamp(fontPixelSize, 8.0f, 96.0f);
		if (std::fabs(clampedSize - mFontPixelSize) < 0.01f) {
			return;
		}
		mFontPixelSize = clampedSize;
		mAscentPx      = 0.0f;
		mInitialized   = false;
	}

	uint32_t UIFontAtlas::GetOversampleH() const {
		return mOversampleH;
	}

	uint32_t UIFontAtlas::GetOversampleV() const {
		return mOversampleV;
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
		if (mTextureAssetId != kInvalidAssetID) {
			assetManager.Release(mTextureAssetId);
			mTextureAssetId = kInvalidAssetID;
		}

		constexpr char kFontPath[] = R"(.\content\core\fonts\JetBrainsMono.ttf)";
		constexpr int  kAsciiCount = static_cast<int>(kAsciiLast - kAsciiFirst + 1);
		std::vector<uint8_t> ttfBytes;
		if (!ReadBinaryFile(kFontPath, ttfBytes)) {
			Warning("UI", "UIFontAtlas failed to read font file: {}", kFontPath);
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

		const float scale = stbtt_ScaleForPixelHeight(&fontInfo, mFontPixelSize);
		int ascent = 0;
		int descent = 0;
		int lineGap = 0;
		stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
		mAscentPx = static_cast<float>(ascent) * scale;
		(void)descent;
		(void)lineGap;

		constexpr int packAtlasWidth  = 2048;
		constexpr int packAtlasHeight = 1024;
		std::vector<uint8_t> packedAtlas(
			static_cast<size_t>(packAtlasWidth) * static_cast<size_t>(packAtlasHeight),
			0
		);
		std::array<stbtt_packedchar, kAsciiCount> packedChars = {};
		stbtt_pack_context packContext = {};
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

		std::array<PendingGlyph, kGlyphTableSize> pending = {};
		int bitmapGlyphCount = 0;
		int zeroBitmapGlyphCount = 0;
		for (uint32_t code = kAsciiFirst; code <= kAsciiLast; ++code) {
			PendingGlyph& glyph = pending[code];
			glyph.codepoint     = static_cast<int>(code);
			const size_t packedIndex = static_cast<size_t>(code - kAsciiFirst);
			glyph.packed = packedChars[packedIndex];
			glyph.advanceX = glyph.packed.xadvance;
			glyph.bearingX = glyph.packed.xoff;
			glyph.bearingY = glyph.packed.yoff;
			glyph.widthPx  = std::max(0.0f, glyph.packed.xoff2 - glyph.packed.xoff);
			glyph.heightPx = std::max(0.0f, glyph.packed.yoff2 - glyph.packed.yoff);
			glyph.srcX0    = glyph.packed.x0;
			glyph.srcY0    = glyph.packed.y0;
			glyph.srcX1    = glyph.packed.x1;
			glyph.srcY1    = glyph.packed.y1;
			glyph.width    = std::max(0, glyph.srcX1 - glyph.srcX0);
			glyph.height   = std::max(0, glyph.srcY1 - glyph.srcY0);
			glyph.hasBitmap = glyph.width > 0 && glyph.height > 0;
			if (glyph.hasBitmap) {
				++bitmapGlyphCount;
			} else {
				++zeroBitmapGlyphCount;
			}
		}

		const int atlasWidth  = packAtlasWidth;
		const int atlasHeight = packAtlasHeight;

		std::vector<uint8_t> rgbaAtlas(
			static_cast<size_t>(atlasWidth) * static_cast<size_t>(atlasHeight) * 4,
			255
		);
		for (size_t i = 0; i < packedAtlas.size(); ++i) {
			rgbaAtlas[i * 4 + 3] = packedAtlas[i];
		}

		TextureAssetData texture = {};
		texture.width            = static_cast<uint32_t>(atlasWidth);
		texture.height           = static_cast<uint32_t>(atlasHeight);
		texture.arraySize        = 1;
		texture.mipLevels        = 1;
		texture.format           = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture.isSRGB           = false;
		texture.sourcePath       = "runtime://ui/font_atlas_ascii";

		TextureMip mip = {};
		mip.width      = texture.width;
		mip.height     = texture.height;
		mip.rowPitch   = static_cast<size_t>(texture.width) * 4;
		mip.bytes      = std::move(rgbaAtlas);
		const size_t atlasRowPitch = mip.rowPitch;
		const size_t atlasDataSize = mip.bytes.size();
		const size_t expectedDataSize = static_cast<size_t>(texture.width) *
		                                static_cast<size_t>(texture.height) * 4;
		if (mip.bytes.size() != expectedDataSize) {
			Warning(
				kChannel,
				"UIFontAtlas RGBA data size mismatch: actual={}, expected={}.",
				mip.bytes.size(),
				expectedDataSize
			);
		}
		texture.mips.emplace_back(std::move(mip));

		mTextureAssetId = assetManager.CreateRuntimeAsset<TextureAssetData>(
			ASSET_TYPE::TEXTURE,
			"runtime.ui.font_atlas_ascii",
			std::move(texture)
		);
		if (mTextureAssetId == kInvalidAssetID) {
			Warning("UI", "UIFontAtlas failed to register runtime texture asset.");
			return false;
		}
		assetManager.AddRef(mTextureAssetId);

		for (uint32_t code = 0; code < mGlyphs.size(); ++code) {
			mGlyphs[code] = {};
		}
		for (uint32_t code = kAsciiFirst; code <= kAsciiLast; ++code) {
			const PendingGlyph& src = pending[code];
			UIGlyph&            dst = mGlyphs[code];
			dst.advanceX            = src.advanceX;
			dst.bearing             = Vec2(
				src.bearingX,
				src.bearingY
			);
			dst.size                = Vec2(
				src.widthPx,
				src.heightPx
			);
			dst.hasBitmap = src.hasBitmap;
			if (!src.hasBitmap) {
				continue;
			}
			const float invWidth  = 1.0f / static_cast<float>(atlasWidth);
			const float invHeight = 1.0f / static_cast<float>(atlasHeight);
			dst.uvMin = Vec2(
				static_cast<float>(src.srcX0) * invWidth,
				static_cast<float>(src.srcY0) * invHeight
			);
			dst.uvMax = Vec2(
				static_cast<float>(src.srcX1) * invWidth,
				static_cast<float>(src.srcY1) * invHeight
			);
		}
		static bool sLoggedGlyphP = false;
		if (!sLoggedGlyphP) {
			const PendingGlyph& glyphP = pending[static_cast<size_t>('P')];
			const UIGlyph& renderedGlyphP =
				mGlyphs[static_cast<size_t>('P')];
			DevMsg(
				kChannel,
				"UIFontAtlas glyph 'P': atlas={}x{}, glyphRect=({}, {}, {}, {}), offset=({}, {}, {}, {}), glyphSize=({}, {}), uvMin=({}, {}), uvMax=({}, {}).",
				atlasWidth,
				atlasHeight,
				glyphP.srcX0,
				glyphP.srcY0,
				glyphP.srcX1,
				glyphP.srcY1,
				glyphP.packed.xoff,
				glyphP.packed.yoff,
				glyphP.packed.xoff2,
				glyphP.packed.yoff2,
				renderedGlyphP.size.x,
				renderedGlyphP.size.y,
				renderedGlyphP.uvMin.x,
				renderedGlyphP.uvMin.y,
				renderedGlyphP.uvMax.x,
				renderedGlyphP.uvMax.y
			);
			sLoggedGlyphP = true;
		}

		DevMsg(
			kChannel,
				"UIFontAtlas initialized: {}x{}, ascii={}..{}, fontSize={}, oversample={}x{}, rowPitch={}, dataSize={}, textureAssetId={}, bitmapGlyphs={}, zeroBitmapGlyphs={}.",
			atlasWidth,
			atlasHeight,
			kAsciiFirst,
			kAsciiLast,
			mFontPixelSize,
			mOversampleH,
			mOversampleV,
			atlasRowPitch,
			atlasDataSize,
			mTextureAssetId,
			bitmapGlyphCount,
			zeroBitmapGlyphCount
		);
		return true;
	}

	UIFontAtlas& GetUIFontAtlas() {
		static UIFontAtlas sAtlas = {};
		return sAtlas;
	}
}
