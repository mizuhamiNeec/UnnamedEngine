#pragma once
#include <functional>
#include <Components/Base/Component.h>

#include "TransformComponent.h"

class ColliderComponent : public Component {
public:
	using CollisionCallback = std::function<void(Entity* other)>;

	explicit ColliderComponent(float radius = 0.5f) : radius_(radius) {}

	~ColliderComponent() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	float GetRadius() const;

	Vec3 GetPosition() const;

	void RegisterCollisionCallback(CollisionCallback callback);

	void OnCollision(Entity* other) const;

private:
	float radius_;
	TransformComponent* transform_ = nullptr;
	std::vector<CollisionCallback> collisionCallbacks_;
};

