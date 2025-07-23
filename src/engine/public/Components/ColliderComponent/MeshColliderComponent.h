#pragma once
#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>

class StaticMesh;
class StaticMeshRenderer;
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

	[[nodiscard]] StaticMesh* GetStaticMesh() const;

private:
	void BuildTriangleList();
	StaticMeshRenderer* mMeshRenderer = nullptr;
	std::vector<Triangle> mTriangles;
};

