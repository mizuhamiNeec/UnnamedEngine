#pragma once

#include <cstdint>

namespace Unnamed::Render {
	/// @brief フルスクリーン描画ルートシグネチャのルートパラメータスロット。
	enum class FS_ROOT_SLOT : uint32_t {
		POST_FX_PARAMS = 0,
		SOURCE_TEXTURE = 1,
	};

	/// @brief ジオメトリ描画ルートシグネチャのルートパラメータスロット。
	enum class GEOM_ROOT_SLOT : uint32_t {
		FRAME = 0,
		OBJECT = 1,
		MATERIAL = 2,
		SKINNING = 3,
		BASE_COLOR_TEXTURE = 4,
		SHADOW_CONSTANTS = 5,
		SHADOW_MAP = 6,
	};

	/// @brief コンピュート描画ルートシグネチャのルートパラメータスロット。
	enum class CS_ROOT_SLOT : uint32_t {
		OUTPUT_TEXTURE = 0,
	};

	/// @brief ルートスロット列挙体をD3D12 APIに渡せるインデックスへ変換します。
	/// @tparam TRootSlot ルートスロット列挙体
	/// @param slot 変換対象スロット
	/// @return ルートパラメータインデックス
	template <typename TRootSlot>
	[[nodiscard]] constexpr uint32_t ToRootIndex(const TRootSlot slot) {
		return static_cast<uint32_t>(slot);
	}
}
