#pragma once
#include <engine/public/Components/base/Component.h>
#include <engine/public/Physics/Physics.h>
#include <math/public/MathLib.h>

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
		mPhysicsEngine = physicsEngine;
	}

	[[nodiscard]] std::vector<HitResult> BoxCast(
		const Vec3& start,
		const Vec3& direction,
		float       distance,
		const Vec3& halfSize
	) const;

	[[nodiscard]] std::vector<HitResult> RayCast(
		const Vec3& start,
		const Vec3& direction,
		float       distance
	) const;

	PhysicsEngine* GetPhysicsEngine() const;

protected:
	PhysicsEngine* mPhysicsEngine = nullptr;
};
