#include "MeshColliderComponent.h"

#include <SubSystem/Console/Console.h>

#include <Physics/Physics.h>

void MeshColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);

	if (auto* smr = mOwner->GetComponent<StaticMeshRenderer>()) {
		mEshRenderer = smr;
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
		if (mEshRenderer) {
			ImGui::Text("StaticMeshRenderer: %s", mEshRenderer->GetStaticMesh()->GetName().c_str());
			ImGui::Text("PolyCount: %d", mEshRenderer->GetStaticMesh()->GetPolygons().size());
		} else {
			ImGui::Text("StaticMeshRenderer: None");
		}
		ImGui::Separator();
	}
}

AABB MeshColliderComponent::GetBoundingBox() const {
	if (!mEshRenderer) {
		Console::Print(
			mOwner->GetName() + " は StaticMeshRenderer がアタッチされていません\n",
			kConTextColorWarning,
			Channel::Physics
		);
		return AABB(Vec3::zero, Vec3::zero);
	}
	const auto& polygons = mEshRenderer->GetStaticMesh()->GetPolygons();
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
	return mTriangles;
}

StaticMesh* MeshColliderComponent::GetStaticMesh() const {
	if (mEshRenderer) {
		return mEshRenderer->GetStaticMesh();
	}
	return nullptr;
}

void MeshColliderComponent::BuildTriangleList() {
	auto* mesh = mEshRenderer->GetStaticMesh();
	if (!mesh) {
		return;
	}

	mTriangles.clear();
	mTriangles = mesh->GetPolygons();
}
