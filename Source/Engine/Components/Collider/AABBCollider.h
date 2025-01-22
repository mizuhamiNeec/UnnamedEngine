#pragma once

#include <Components/Collider/Base/Collider.h>

#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Physics/Physics.h>

class AABBCollider : public Collider {
public:
	AABBCollider(const Vec3& size);
	void Update(float deltaTime) override;

	void DrawInspectorImGui() override;

	ColliderType GetColliderType() override;

	void OnAttach(Entity& owner) override;

	[[nodiscard]] Physics::AABB GetAABB() const;

private:
	Physics::AABB aabb_;
	Vec3 size_;
};

