#include "UIDrawCommandSprite.h"

#include <algorithm>
#include <cmath>

#include "UIFontAtlas.h"

#include "core/assets/AssetManager.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::UI {
	namespace {
		constexpr bool             kSnapTextGlyphToPixel = true;
		constexpr std::string_view kDefaultUIFontPath    =
			R"(.\content\core\fonts\JetBrainsMono.ttf)";

		[[nodiscard]] Render::ScreenSpriteInput BuildRectSprite(
			const UIDrawCommand& command, const int32_t sortKey
		) {
			Render::ScreenSpriteInput sprite = {};
			sprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			// kInvalidAssetID は Renderer 側で 1x1 white fallback + tint 描画に解決されます。
			sprite.texture.textureAssetId = kInvalidAssetID;
			// ScreenSprites は positionPx と anchor で中心を計算するため、(0,0) は左上基準です。
			sprite.positionPx  = command.rect.position;
			sprite.sizePx      = command.rect.size;
			sprite.anchor      = Vec2(0.0f, 0.0f);
			sprite.rotationRad = 0.0f;
			sprite.color       = Vec4(
				command.color.r,
				command.color.g,
				command.color.b,
				command.color.a
			);
			sprite.sortKey = sortKey;
			sprite.uvMin   = Vec2(0.0f, 0.0f);
			sprite.uvMax   = Vec2(1.0f, 1.0f);
			sprite.uvFlipY = true;
			return sprite;
		}
	}

	void AppendDrawCommandScreenSprites(
		const UIDrawCommand&                    command,
		const int32_t                           baseSortKey,
		AssetManager&                           assetManager,
		std::vector<Render::ScreenSpriteInput>& outSprites,
		UIDrawCommandSpriteStats*               outStats
	) {
		if (command.type == UIDrawCommandType::RECT) {
			outSprites.emplace_back(BuildRectSprite(command, baseSortKey));
			return;
		}

		if (command.type != UIDrawCommandType::TEXT || command.text.empty()) {
			return;
		}
		if (outStats != nullptr) {
			++outStats->textCommandCount;
		}

		UIFontAtlasCache&    fontAtlasCache = GetUIFontAtlasCache();
		const UIFontAtlasKey key            = MakeUIFontAtlasKey(
			Path(kDefaultUIFontPath),
			command.textFontSize,
			command.textOversampleH,
			command.textOversampleV
		);
		UIFontAtlas* fontAtlas = fontAtlasCache.GetOrCreate(key, assetManager);
		if (fontAtlas == nullptr) {
			return;
		}

		bool forceFallbackTexture = false;
		int  textDebugMode        = 0;
		if (
			auto* console = ServiceLocator::Get<ConsoleSystem>();
			console != nullptr
		) {
			if (
				const auto* forceFallbackVar =
					console->GetConVarAs<ConVar<bool>>(
						"ui_new_text_force_fallback_texture"
					);
				forceFallbackVar != nullptr
			) {
				forceFallbackTexture = forceFallbackVar->GetValue();
			}
			if (
				const auto* textDebugModeVar =
					console->GetConVarAs<ConVar<int>>(
						"ui_new_text_debug_mode"
					);
				textDebugModeVar != nullptr
			) {
				textDebugMode = textDebugModeVar->GetValue();
			}
		}

		static float    sLoggedFontSize      = -1.0f;
		static uint32_t sLoggedOversampleH   = 0;
		static uint32_t sLoggedOversampleV   = 0;
		static bool     sLoggedForceFallback = false;
		const bool      atlasStateChanged    =
			std::fabs(sLoggedFontSize - fontAtlas->GetFontPixelSize()) > 0.01f
			||
			sLoggedOversampleH != fontAtlas->GetOversampleH() ||
			sLoggedOversampleV != fontAtlas->GetOversampleV() ||
			sLoggedForceFallback != forceFallbackTexture;
		if (textDebugMode >= 1 && atlasStateChanged) {
			const UIFontAtlasCacheDebugInfo cacheInfo = fontAtlasCache.
				GetDebugInfo();
			DevMsg(
				"UI",
				"Text runtime atlas state: textureAssetId={}, fontSize={}, oversample={}x{}, forceFallbackTexture={}, debugMode={}, cacheCount={}, createCalls={}.",
				fontAtlas->GetTextureAssetId(),
				fontAtlas->GetFontPixelSize(),
				fontAtlas->GetOversampleH(),
				fontAtlas->GetOversampleV(),
				forceFallbackTexture,
				textDebugMode,
				cacheInfo.cacheCount,
				cacheInfo.createRuntimeAssetCallCount
			);
			sLoggedFontSize      = fontAtlas->GetFontPixelSize();
			sLoggedOversampleH   = fontAtlas->GetOversampleH();
			sLoggedOversampleV   = fontAtlas->GetOversampleV();
			sLoggedForceFallback = forceFallbackTexture;
		}

		static bool sLoggedGlyphLookupPlay = false;
		if (textDebugMode >= 1 && !sLoggedGlyphLookupPlay) {
			const UIGlyph* glyphP = fontAtlas->FindGlyph('P');
			const UIGlyph* glyphL = fontAtlas->FindGlyph('l');
			const UIGlyph* glyphA = fontAtlas->FindGlyph('a');
			const UIGlyph* glyphY = fontAtlas->FindGlyph('y');
			DevMsg(
				"UI",
				"Glyph lookup check: P(has={},size={}x{}), l(has={},size={}x{}), a(has={},size={}x{}), y(has={},size={}x{}).",
				glyphP ? glyphP->hasBitmap : false,
				glyphP ? glyphP->size.x : 0.0f,
				glyphP ? glyphP->size.y : 0.0f,
				glyphL ? glyphL->hasBitmap : false,
				glyphL ? glyphL->size.x : 0.0f,
				glyphL ? glyphL->size.y : 0.0f,
				glyphA ? glyphA->hasBitmap : false,
				glyphA ? glyphA->size.x : 0.0f,
				glyphA ? glyphA->size.y : 0.0f,
				glyphY ? glyphY->hasBitmap : false,
				glyphY ? glyphY->size.x : 0.0f,
				glyphY ? glyphY->size.y : 0.0f
			);
			sLoggedGlyphLookupPlay = true;
		}

		float       cursorX   = std::round(command.textPosition.x);
		const float baselineY =
			std::round(command.textPosition.y + fontAtlas->GetAscentPx());
		int32_t glyphSortOffset = 0;

		for (const char character : command.text) {
			const UIGlyph* glyph = fontAtlas->FindGlyph(character);
			if (glyph == nullptr) {
				if (outStats != nullptr) {
					++outStats->skippedGlyphCount;
				}
				continue;
			}
			if (glyph->hasBitmap) {
				Render::ScreenSpriteInput sprite = {};
				sprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
				sprite.texture.textureAssetId = forceFallbackTexture ?
					                                kInvalidAssetID :
					                                fontAtlas->
					                                GetTextureAssetId();
				const auto glyphPosition = Vec2(
					std::round(cursorX + glyph->bearing.x),
					std::round(baselineY + glyph->bearing.y)
				);
				const Vec2 glyphSize = glyph->size;
				sprite.positionPx    = kSnapTextGlyphToPixel ?
					                    Vec2(
						                    std::round(glyphPosition.x),
						                    std::round(glyphPosition.y)
					                    ) :
					                    glyphPosition;
				sprite.sizePx = kSnapTextGlyphToPixel ?
					                Vec2(
						                std::max(1.0f, std::round(glyphSize.x)),
						                std::max(1.0f, std::round(glyphSize.y))
					                ) :
					                glyphSize;
				sprite.anchor      = Vec2(0.0f, 0.0f);
				sprite.rotationRad = 0.0f;
				sprite.color       = Vec4(
					command.color.r,
					command.color.g,
					command.color.b,
					1.0f
				);
				if (forceFallbackTexture) {
					sprite.color = Vec4(1.0f, 0.2f, 0.1f, 1.0f);
				}
				sprite.sortKey = baseSortKey + glyphSortOffset;
				// CPU 側の UV は atlas/image 上の自然な向きで保持します。
				// 反転責務は ScreenSpriteInput::uvFlipY を使って RendererGraph 側で統一します。
				sprite.uvMin   = glyph->uvMin;
				sprite.uvMax   = glyph->uvMax;
				sprite.uvFlipY = true;

				const bool forceDebugGlyphRect = textDebugMode == 4 &&
				                                 outStats != nullptr &&
				                                 outStats->glyphSpriteCount ==
				                                 0;
				if (forceDebugGlyphRect) {
					sprite.positionPx = Vec2(320.0f, 32.0f);
					sprite.sizePx = Vec2(64.0f, 64.0f);
					sprite.texture.textureAssetId = kInvalidAssetID;
					sprite.uvMin = Vec2(0.0f, 0.0f);
					sprite.uvMax = Vec2(1.0f, 1.0f);
					sprite.color = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
					sprite.anchor = Vec2(0.0f, 0.0f);
					sprite.uvFlipY = true;
					Warning(
						"UI",
						"ui_new_text_debug_mode=4 applied to first glyph '{}': forced red 64x64 fallback rect.",
						character
					);
				}
				static bool sLoggedGlyphSpriteP = false;
				if (
					textDebugMode >= 1 && !sLoggedGlyphSpriteP &&
					character == 'P'
				) {
					DevMsg(
						"UI",
						"UIFontAtlas glyph 'P' sprite: atlasGlyphSize=({}, {}), glyphOffset=({}, {}), finalPosition=({}, {}), finalSize=({}, {}), uvMin=({}, {}), uvMax=({}, {}).",
						glyph->size.x,
						glyph->size.y,
						glyph->bearing.x,
						glyph->bearing.y,
						sprite.positionPx.x,
						sprite.positionPx.y,
						sprite.sizePx.x,
						sprite.sizePx.y,
						sprite.uvMin.x,
						sprite.uvMin.y,
						sprite.uvMax.x,
						sprite.uvMax.y
					);
					sLoggedGlyphSpriteP = true;
				}

				static bool sLoggedFirstGlyphSprite = false;
				if (textDebugMode >= 1 && !sLoggedFirstGlyphSprite) {
					DevMsg(
						"UI",
						"First glyph sprite: ch='{}', pos=({}, {}), size=({}, {}), anchor=({}, {}), uvFlipY={}, color=({}, {}, {}, {}), texAssetId={}, uvMin=({}, {}), uvMax=({}, {}).",
						character,
						sprite.positionPx.x,
						sprite.positionPx.y,
						sprite.sizePx.x,
						sprite.sizePx.y,
						sprite.anchor.x,
						sprite.anchor.y,
						sprite.uvFlipY,
						sprite.color.x,
						sprite.color.y,
						sprite.color.z,
						sprite.color.w,
						sprite.texture.textureAssetId,
						sprite.uvMin.x,
						sprite.uvMin.y,
						sprite.uvMax.x,
						sprite.uvMax.y
					);
					DevMsg(
						"UI",
						"First glyph sprite extra: opacity(derived)= {}, sortKey={}, rotationRad={}, textureSource={}, layer/order fields are not present in ScreenSpriteInput.",
						sprite.color.w,
						sprite.sortKey,
						sprite.rotationRad,
						static_cast<int>(sprite.texture.source)
					);
					sLoggedFirstGlyphSprite = true;
				}
				outSprites.emplace_back(std::move(sprite));
				if (outStats != nullptr) {
					++outStats->glyphSpriteCount;
				}
			} else if (outStats != nullptr) {
				++outStats->skippedGlyphCount;
			}
			cursorX += glyph->advanceX;
			++glyphSortOffset;
		}
	}
}
