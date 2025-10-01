#include <engine/ResourceSystem/Mesh/StaticMesh.h>

void StaticMesh::AddSubMesh(std::unique_ptr<SubMesh> subMesh) {
	subMeshes_.emplace_back(std::move(subMesh));
}

const std::vector<std::unique_ptr<SubMesh>>& StaticMesh::GetSubMeshes() const {
	return subMeshes_;
}

std::string StaticMesh::GetName() const { return name_; }

std::vector<Unnamed::Triangle> StaticMesh::GetPolygons() const {
	std::vector<Unnamed::Triangle> polygons;
	for (const auto& subMesh : subMeshes_) {
		auto subMeshPolygons = subMesh->GetPolygons();
		polygons.insert(polygons.end(), subMeshPolygons.begin(),
		                subMeshPolygons.end());
	}
	return polygons;
}

void StaticMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	for (const auto& subMesh : subMeshes_) {
		subMesh->Render(commandList);
	}
}

void StaticMesh::ReleaseResource() {
	for (auto& subMesh : subMeshes_) {
		subMesh->ReleaseResource();
	}
	subMeshes_.clear();
}
