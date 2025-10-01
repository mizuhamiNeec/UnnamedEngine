#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

class PhysicsEngine;

struct HitResult;

class ColliderComponent : public Component {
public:
	void Update(float deltaTime) override = 0;
	void DrawInspectorImGui() override = 0;

	virtual bool CheckCollision(const ColliderComponent* other) const = 0;
	virtual bool IsDynamic() = 0;

	void SetPhysicsEngine(PhysicsEngine* physicsEngine) {
		mPhysicsEngine = physicsEngine;
	}

	PhysicsEngine* GetPhysicsEngine() const;

protected:
	PhysicsEngine* mPhysicsEngine = nullptr;
};
