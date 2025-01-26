#include "MeshColliderComponent.h"

#include <Lib/Console/Console.h>

#include <Physics/Physics.h>

void MeshColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);

	if (auto* smr = owner_->GetComponent<StaticMeshRenderer>()) {
		meshRenderer_ = smr;
		BuildTriangleList();
	} else {
		Console::Print(
			owner_->GetName() + " は StaticMeshRenderer がアタッチされていません\n",
			kConsoleColorWarning,
			Channel::Physics
		);
	}
}

void MeshColliderComponent::Update(float deltaTime) {
	deltaTime;
}

void MeshColliderComponent::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader("MeshColliderComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (meshRenderer_) {
			ImGui::Text("StaticMeshRenderer: %s", meshRenderer_->GetStaticMesh()->GetName().c_str());
			ImGui::Text("PolyCount: %d", meshRenderer_->GetStaticMesh()->GetPolygons().size());
		} else {
			ImGui::Text("StaticMeshRenderer: None");
		}
		ImGui::Separator();
	}
}

AABB MeshColliderComponent::GetBoundingBox() const {
	if (!meshRenderer_) {
		Console::Print(
			owner_->GetName() + " は StaticMeshRenderer がアタッチされていません\n",
			kConsoleColorWarning,
			Channel::Physics
		);
		return AABB(Vec3::zero, Vec3::zero);
	}
	const auto& polygons = meshRenderer_->GetStaticMesh()->GetPolygons();
	if (polygons.empty()) {
		return AABB(Vec3::zero, Vec3::zero);
	}
	Vec3 min = polygons[0].v0;
	Vec3 max = polygons[0].v0;
	for (const auto& tri : polygons) {
		min = Vec3::Min(min, tri.v0);
		min = Vec3::Min(min, tri.v1);
		min = Vec3::Min(min, tri.v2);
		max = Vec3::Max(max, tri.v0);
		max = Vec3::Max(max, tri.v1);
		max = Vec3::Max(max, tri.v2);
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
	return triangles_;
}

void MeshColliderComponent::BuildTriangleList() {
	auto* mesh = meshRenderer_->GetStaticMesh();
	if (!mesh) {
		return;
	}

	triangles_.clear();
	triangles_ = mesh->GetPolygons();
}
