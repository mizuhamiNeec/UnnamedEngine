#pragma once

namespace Unnamed {
	/// @brief EditorUiMetricsは、DPI倍率に応じたEditor UIの寸法と間隔を算出します
	class EditorUiMetrics {
	public:
		void SetScale(const float scale) {
			mScale = scale;
		}

		[[nodiscard]] float Dp(const float value) const {
			return value * mScale;
		}

	private:
		float mScale = 1.0f;
	};
}
