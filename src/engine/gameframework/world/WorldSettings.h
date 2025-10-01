#pragma once
#include <runtime/core/math/Math.h>

/// @brief ワールドの設定
struct WorldSettings {
	/// 大まかなワールドの境界で、外に出ようとしたEntityは範囲内に戻されます
	/// Vec3::zero が設定されている場合、境界は無効になります
	Vec3 worldBounds = Vec3::zero;
};
