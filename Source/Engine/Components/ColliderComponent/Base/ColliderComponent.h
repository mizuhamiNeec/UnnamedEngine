#pragma once
#include <Components/Base/Component.h>
#include <SubSystem/Console/Console.h>

#include "Physics/Physics.h"

class PhysicsEngine;

struct HitResult;

class ColliderComponent : public Component {
public:
	void Update(float deltaTime) override = 0;
	void DrawInspectorImGui() override = 0;

	[[nodiscard]] virtual AABB GetBoundingBox() const = 0;
	virtual bool CheckCollision(const ColliderComponent* other) const = 0;
	virtual bool IsDynamic() = 0;

	void SetPhysicsEngine(PhysicsEngine* physicsEngine) {
		physicsEngine_ = physicsEngine;
	}

	[[nodiscard]] std::vector<HitResult> BoxCast(
		const Vec3& start,
		const Vec3& direction,
		float distance,
		const Vec3& halfSize
	) const;

	[[nodiscard]] std::vector<HitResult> RayCast(
		const Vec3& start,
		const Vec3& direction,
		float distance
	) const;

protected:
	PhysicsEngine* physicsEngine_ = nullptr;
};
