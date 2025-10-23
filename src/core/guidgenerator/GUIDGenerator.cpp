#include "GuidGenerator.h"

#include <array>


/// @brief GuidGeneratorのコンストラクタ
/// @param m 生成モード
/// @details シーケンシャルモードの場合、乱数生成器を初期化します
GuidGenerator::GuidGenerator(const MODE m) : mMode(m) {
	if (mMode == MODE::SEQUENTIAL) {
		std::random_device rd;
		std::array         seed{rd(), rd()};
		mRng.seed(*reinterpret_cast<uint64_t*>(seed.data()));
	}
}


/// @brief 新しいGUIDを割り当てる
/// @return 生成された64ビットGUID
/// @details シーケンシャルモードでは連番、ランダムモードでは乱数を生成します
uint64_t GuidGenerator::Alloc() {
	switch (mMode) {
	case MODE::SEQUENTIAL:
		return ++mCounter;
	case MODE::RANDOM64:
		return mDist64(mRng) | 0x8000'0000'0000'0000ULL;
	default:
		return ++mCounter;
	}
}
