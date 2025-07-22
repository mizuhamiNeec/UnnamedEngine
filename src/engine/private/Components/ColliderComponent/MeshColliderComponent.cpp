#include <engine/public/Components/ColliderComponent/MeshColliderComponent.h>

#include "engine/public/Components/MeshRenderer/StaticMeshRenderer.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/OldConsole/Console.h"
#include "engine/public/ResourceSystem/Mesh/StaticMesh.h"

void MeshColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);

	if (auto* smr = mOwner->GetComponent<StaticMeshRenderer>()) {
		mMeshRenderer = smr;
		BuildTriangleList();
	} else {
		Console::Print(
			mOwner->GetName() + " は StaticMeshRenderer がアタッチされていません\n",
			kConTextColorWarning,
			Channel::Physics
		);
	}
}

void MeshColliderComponent::Update(float deltaTime) {
	deltaTime;
}

void MeshColliderComponent::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader("MeshColliderComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (mMeshRenderer) {
			ImGui::Text("StaticMeshRenderer: %s", mMeshRenderer->GetStaticMesh()->GetName().c_str());
			ImGui::Text("PolyCount: %d", mMeshRenderer->GetStaticMesh()->GetPolygons().size());
		} else {
			ImGui::Text("StaticMeshRenderer: None");
		}
		ImGui::Separator();
	}
}

AABB MeshColliderComponent::GetBoundingBox() const {
	if (!mMeshRenderer) {
		Console::Print(
			mOwner->GetName() + " は StaticMeshRenderer がアタッチされていません\n",
			kConTextColorWarning,
			Channel::Physics
		);
		return AABB(Vec3::zero, Vec3::zero);
	}
	const auto& polygons = mMeshRenderer->GetStaticMesh()->GetPolygons();
	if (polygons.empty()) {
		return AABB(Vec3::zero, Vec3::zero);
	}
	Vec3 min = polygons[0].v0;
	Vec3 max = polygons[0].v0;
	for (const auto& tri : polygons) {
		min = Math::Min(min, tri.v0);
		min = Math::Min(min, tri.v1);
		min = Math::Min(min, tri.v2);
		max = Math::Max(max, tri.v0);
		max = Math::Max(max, tri.v1);
		max = Math::Max(max, tri.v2);
	}
	return AABB(min, max);
}

bool MeshColliderComponent::CheckCollision(const ColliderComponent* other) const {
	other;
	return false;
}

bool MeshColliderComponent::IsDynamic() {
	return false;
}

std::vector<Triangle> MeshColliderComponent::GetTriangles() {
	return mTriangles;
}

StaticMesh* MeshColliderComponent::GetStaticMesh() const {
	if (mMeshRenderer) {
		return mMeshRenderer->GetStaticMesh();
	}
	return nullptr;
}

void MeshColliderComponent::BuildTriangleList() {
	auto* mesh = mMeshRenderer->GetStaticMesh();
	if (!mesh) {
		return;
	}

	mTriangles.clear();
	mTriangles = mesh->GetPolygons();
}
