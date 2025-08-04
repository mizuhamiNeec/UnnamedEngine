#pragma once
#include <vector>

#include <engine/public/uphysics/BVHBuilder.h>

class Entity;

struct BVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t>           triIndices;
};

struct RegisteredBVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t> triIndices;

	size_t  triStart;
	size_t  triCount;
	Entity* owner;
};
