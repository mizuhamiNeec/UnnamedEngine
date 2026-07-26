#pragma once
#include <cstdint>
#include <vector>

#include <core/math/Mat4.h>
#include <engine/unnamed/physics/BVHBuilder.h>

/// @brief 三角形群を階層化した境界ボリュームと葉インデックスを所有します
struct BVH {
	std::vector<Unnamed::Physics::FlatNode> nodes;
	std::vector<uint32_t>                   triIndices;
};

enum class ColliderMobility : uint8_t {
	Static,
	Dynamic
};

/// @brief 登録されたBVH構造体
struct RegisteredBVH {
	std::vector<Unnamed::Physics::FlatNode> nodes;
	std::vector<uint32_t>                   triIndices;
	std::vector<Unnamed::Triangle>          triangles;

	uint64_t         ownerGuid = 0;
	ColliderMobility mobility  = ColliderMobility::Static;
	Mat4             world     = Mat4::identity;
};
