#include <engine/ResourceSystem/Mesh/StaticMesh.h>

/// @brief コンストラクタ
/// @param name メッシュ名
void StaticMesh::AddSubMesh(std::unique_ptr<SubMesh> subMesh) {
	subMeshes_.emplace_back(std::move(subMesh));
}

/// @brief サブメッシュの取得
/// @return サブメッシュのベクターへの参照
const std::vector<std::unique_ptr<SubMesh>>& StaticMesh::GetSubMeshes() const {
	return subMeshes_;
}

/// @brief メッシュ名の取得
/// @return メッシュ名
std::string StaticMesh::GetName() const { return name_; }

/// @brief ポリゴン情報の取得
/// @return ポリゴン情報のベクター
std::vector<Unnamed::Triangle> StaticMesh::GetPolygons() const {
	std::vector<Unnamed::Triangle> polygons;
	for (const auto& subMesh : subMeshes_) {
		auto subMeshPolygons = subMesh->GetPolygons();
		polygons.insert(polygons.end(), subMeshPolygons.begin(),
		                subMeshPolygons.end());
	}
	return polygons;
}

/// @brief レンダリング
/// @param commandList コマンドリスト
void StaticMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	for (const auto& subMesh : subMeshes_) {
		subMesh->Render(commandList);
	}
}

/// @brief リソースの解放
void StaticMesh::ReleaseResource() {
	for (auto& subMesh : subMeshes_) {
		subMesh->ReleaseResource();
	}
	subMeshes_.clear();
}
