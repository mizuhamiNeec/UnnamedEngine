#pragma once

#include <cstdint>
#include <vector>

#include "UnnamedUIDrawList.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed::UI {
	class UIFontAtlas;
	struct UIDrawCommandSpriteStats {
		uint32_t textCommandCount  = 0;
		uint32_t glyphSpriteCount  = 0;
		uint32_t skippedGlyphCount = 0;
	};

	/// @brief UI描画コマンドを画面スプライト列へ変換して追加します。
	/// @details TEXT はグリフごとに分解されます。
	void AppendDrawCommandScreenSprites(
		const UIDrawCommand&                    command,
		int32_t                                 baseSortKey,
		const UIFontAtlas*                      fontAtlas,
		std::vector<Render::ScreenSpriteInput>& outSprites,
		UIDrawCommandSpriteStats*               outStats = nullptr
	);
}
