#pragma once

#include <iterator>
#include <utility>
#include <vector>

#include "RenderFrameInputs.h"

namespace Unnamed::Render {
	/// @brief オーバーレイ描画用のフレーム投入データです。
	struct RenderOverlayFrameData {
		std::vector<ScreenSpriteInput> screenSprites;

		void Reset() {
			screenSprites.clear();
		}
	};

	/// @brief フレーム単位のレンダー投入データを集約します。
	struct RenderFrameContext {
		/// @brief フレーム投入データを初期化します。
		void Reset() {
			mOverlayData.Reset();
		}

		/// @brief オーバーレイスプライトを1件追加します。
		void AddOverlaySprite(const ScreenSpriteInput& sprite) {
			mOverlayData.screenSprites.emplace_back(sprite);
		}

		/// @brief オーバーレイスプライトを1件追加します（ムーブ版）。
		void AddOverlaySprite(ScreenSpriteInput&& sprite) {
			mOverlayData.screenSprites.emplace_back(std::move(sprite));
		}

		/// @brief オーバーレイスプライトを複数追加します。
		void AddOverlaySprites(const std::vector<ScreenSpriteInput>& sprites) {
			mOverlayData.screenSprites.insert(
				mOverlayData.screenSprites.end(),
				sprites.begin(),
				sprites.end()
			);
		}

		/// @brief オーバーレイスプライトを複数追加します（ムーブ版）。
		void AddOverlaySprites(std::vector<ScreenSpriteInput>&& sprites) {
			if (sprites.empty()) {
				return;
			}
			mOverlayData.screenSprites.insert(
				mOverlayData.screenSprites.end(),
				std::make_move_iterator(sprites.begin()),
				std::make_move_iterator(sprites.end())
			);
		}

		/// @brief 現在フレームのオーバーレイ投入データを返します。
		[[nodiscard]] const RenderOverlayFrameData& GetOverlayData() const {
			return mOverlayData;
		}

	private:
		RenderOverlayFrameData mOverlayData;
	};
}
