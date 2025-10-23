#include <engine/Components/ColliderComponent/MeshColliderComponent.h>

#include "engine/Components/MeshRenderer/StaticMeshRenderer.h"
#include "engine/Entity/Entity.h"
#include "engine/OldConsole/Console.h"
#include "engine/ResourceSystem/Mesh/StaticMesh.h"

/// @brief エンティティにアタッチされたときの処理
/// @param owner アタッチされたエンティティ
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

/// @brief 毎フレームの更新処理
/// @param deltaTime 前フレームからの経過時間
void MeshColliderComponent::Update(float deltaTime) {
	deltaTime;
}

/// @brief ImGuiでインスペクターを描画する
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

/// @brief 他のコライダーとの衝突判定
/// @param other 他のコライダーコンポーネント
/// @return 衝突しているかどうか
bool MeshColliderComponent::CheckCollision(
	const ColliderComponent* other) const {
	other;
	return false;
}

/// @brief 動的コライダーかどうかを返す
/// @return 動的コライダーであればtrue、静的コライダーであればfalse
bool MeshColliderComponent::IsDynamic() {
	return false;
}

/// @brief 三角形リストを取得する
/// @return 三角形リスト
std::vector<Unnamed::Triangle> MeshColliderComponent::GetTriangles() {
	return mTriangles;
}

/// @brief 静的メッシュを取得する
/// @return 静的メッシュのポインタ
StaticMesh* MeshColliderComponent::GetStaticMesh() const {
	if (mMeshRenderer) {
		return mMeshRenderer->GetStaticMesh();
	}
	return nullptr;
}

/// @brief 三角形リストを構築する
void MeshColliderComponent::BuildTriangleList() {
	auto* mesh = mMeshRenderer->GetStaticMesh();
	if (!mesh) {
		return;
	}

	mTriangles.clear();
	mTriangles = mesh->GetPolygons();
}
