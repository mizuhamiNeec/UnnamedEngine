#pragma once

#include <Components/ColliderComponent/Base/ColliderComponent.h>

#include <Components/MeshRenderer/StaticMeshRenderer.h>

struct AABB;

class MeshColliderComponent : public ColliderComponent {
public:
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	[[nodiscard]] AABB GetBoundingBox() const override;
	bool CheckCollision(const ColliderComponent* other) const override;
	bool IsDynamic() override;
	std::vector<Triangle> GetTriangles();

private:
	void BuildTriangleList();
	StaticMeshRenderer* meshRenderer_ = nullptr;
	std::vector<Triangle> triangles_;
};

