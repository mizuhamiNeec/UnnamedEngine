#pragma once

#include <atomic>
#include <cstdint>

namespace Unnamed {
	/// @brief GUIDを生成するクラス
	/// @details シーケンシャルまたはランダムな64ビット識別子を生成します
	class GuidGenerator {
	public:
		/// @brief GUID生成モード	
		enum class MODE {
			SEQUENTIAL, ///< シーケンシャル（連番）モード
			RANDOM64    ///< 64ビットランダムモード
		};

		/// @brief コンストラクタ
		/// @param m 生成モード（デフォルト: SEQUENTIAL）
		explicit GuidGenerator(MODE m = MODE::RANDOM64);

		/// @brief 新しいGUIDを割り当てる
		/// @return 生成された64ビットGUID
		uint64_t Alloc();

		/// @brief スレッドローカル乱数生成器から64ビットGUIDを割り当てる
		/// @return 最上位ビットがセットされたランダムな64ビットGUID
		[[nodiscard]] static uint64_t AllocRandom();

	private:
		std::atomic<uint64_t> mCounter = {0};
		MODE                  mMode;
	};
}
