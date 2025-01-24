#pragma once
#include "Physics.h"

//-----------------------------------------------------------------------------
// 物理オブジェクト
//-----------------------------------------------------------------------------
class PhysicsObject {
public:
	PhysicsObject(int id, const Vec3& position, const Vec3& size, float mass) : id(id),
		position(position),
		velocity(Vec3::zero),
		mass(mass),
		boundingBox(Vec3::zero, Vec3::zero) {
		Vec3 halfSize = size * 0.5f;
		boundingBox = { position - halfSize, position + halfSize };
	}

	int id;
	Vec3 position;
	Vec3 velocity;

	float mass;
	Physics::AABB boundingBox;
};

