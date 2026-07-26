#pragma once
#include <core/math/Vec2.h>

namespace Unnamed::Gui {
	/// @brief UI空間の左上座標と幅・高さをピクセル単位で表します
	struct Rect {
		float x      = 0.0f; // X座標
		float y      = 0.0f; // Y座標
		float width  = 0.0f; // 幅
		float height = 0.0f; // 高さ

		[[nodiscard]] Vec2 Position() const {
			return {x, y};
		}

		[[nodiscard]] Vec2 Size() const {
			return {width, height};
		}

		[[nodiscard]] Vec2 Center() const {
			return {x + width * 0.5f, y + height * 0.5f};
		}
	};

	/// @brief アンカー構造体
	/// @details 親矩形に対する相対位置を0.0〜1.0で表現します。
	struct Anchors {
		// 0.0〜1.0
		float minX = 0.0f; // 左
		float minY = 0.0f; // 上
		float maxX = 0.0f; // 右
		float maxY = 0.0f; // 下
	};

	/// @brief マージン構造体
	/// @details アンカーからのオフセット
	struct Margins {
		float left   = 0.0f;
		float top    = 0.0f;
		float right  = 0.0f;
		float bottom = 0.0f;
	};

	/// @brief ピボット構造体
	/// @details 要素の基準点を0.0〜1.0で表現します。
	///   (0,0)────────(1,0)
	///     │            │
	///     │     ·      │  ← (0.5, 0.5) 中心
	///     │            │
	///   (0,1)────────(1,1)
	struct Pivot {
		float x = 0.0f;
		float y = 0.0f;
	};
}
