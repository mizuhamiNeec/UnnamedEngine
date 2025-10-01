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
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("MeshColliderComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (mMeshRenderer) {
			ImGui::Text("StaticMeshRenderer: %s",
			            mMeshRenderer->GetStaticMesh()->GetName().c_str());
			ImGui::Text("PolyCount: %d",
			            mMeshRenderer->GetStaticMesh()->GetPolygons().size());
		} else {
			ImGui::Text("StaticMeshRenderer: None");
		}
		ImGui::Separator();
	}
#endif
}

bool MeshColliderComponent::CheckCollision(
	const ColliderComponent* other) const {
	other;
	return false;
}

bool MeshColliderComponent::IsDynamic() {
	return false;
}

std::vector<Unnamed::Triangle> MeshColliderComponent::GetTriangles() {
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
