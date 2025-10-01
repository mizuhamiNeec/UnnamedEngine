#pragma once
#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>

#include "engine/public/uprimitive/UPrimitives.h"

class StaticMesh;
class StaticMeshRenderer;
struct AABB;

class MeshColliderComponent : public ColliderComponent {
public:
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	bool CheckCollision(const ColliderComponent* other) const override;
	bool IsDynamic() override;
	std::vector<Unnamed::Triangle> GetTriangles();

	[[nodiscard]] StaticMesh* GetStaticMesh() const;

private:
	void                           BuildTriangleList();
	StaticMeshRenderer*            mMeshRenderer = nullptr;
	std::vector<Unnamed::Triangle> mTriangles;
};
