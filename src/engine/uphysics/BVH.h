#pragma once
#include <vector>

#include <engine/uphysics/BVHBuilder.h>

class Entity;

/// @brief BVH構造体
struct BVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t>           triIndices;
};

/// @brief 登録されたBVH構造体
struct RegisteredBVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t>           triIndices;

	size_t  triStart;
	size_t  triCount;
	Entity* owner;
};
