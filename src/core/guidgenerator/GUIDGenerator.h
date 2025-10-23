#pragma once

#include <atomic>
#include <cstdint>
#include <random>


/// @brief 一意識別子（GUID）を生成するクラス
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
	explicit GuidGenerator(MODE m = MODE::SEQUENTIAL);

	/// @brief 新しいGUIDを割り当てる
	/// @return 生成された64ビットGUID
	uint64_t Alloc();

private:
	std::atomic<uint64_t> mCounter = {0};

	MODE                                    mMode;
	std::mt19937_64                         mRng;
	std::uniform_int_distribution<uint64_t> mDist64;
};
