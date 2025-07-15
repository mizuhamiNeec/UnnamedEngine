#include "SubMesh.h"

#include "SubSystem/Console/Console.h"

#include <Physics/Physics.h>

SubMesh::SubMesh(const ComPtr<ID3D12Device>& device, std::string name) :
	name_(std::move(name)),
	device_(device) {
}

SubMesh::~SubMesh() {}

void SubMesh::SetVertexBuffer(const std::vector<Vertex>& vertices) {
	size_t size = vertices.size() * sizeof(Vertex);
	vertexBuffer_ = std::make_unique<VertexBuffer<Vertex>>(device_, size, vertices.data());
	isSkinnedMesh_ = false;
}

void SubMesh::SetSkinnedVertexBuffer(const std::vector<SkinnedVertex>& vertices) {
	size_t size = vertices.size() * sizeof(SkinnedVertex);
	skinnedVertexBuffer_ = std::make_unique<VertexBuffer<SkinnedVertex>>(device_, size, vertices.data());
	isSkinnedMesh_ = true;
}

void SubMesh::SetIndexBuffer(const std::vector<uint32_t>& indices) {
	size_t size = indices.size() * sizeof(uint32_t);
	indexBuffer_ = std::make_unique<IndexBuffer>(device_, size, indices.data());
}

Material* SubMesh::GetMaterial() const { return material_; }

void SubMesh::SetMaterial(Material* material) { material_ = material; }

std::string& SubMesh::GetName() {
	return name_;
}

void SubMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	// 頂点バッファとインデックスバッファを設定
	if (isSkinnedMesh_) {
		if (!skinnedVertexBuffer_ || !indexBuffer_) {
			Console::Print("スキニング頂点バッファまたはインデックスバッファが設定されていません", kConTextColorError, Channel::RenderSystem);
			return;
		}
		const D3D12_VERTEX_BUFFER_VIEW vbView = skinnedVertexBuffer_->View();
		commandList->IASetVertexBuffers(0, 1, &vbView);
	} else {
		if (!vertexBuffer_ || !indexBuffer_) {
			Console::Print("頂点バッファまたはインデックスバッファが設定されていません", kConTextColorError, Channel::RenderSystem);
			return;
		}
		const D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
		commandList->IASetVertexBuffers(0, 1, &vbView);
	}
	
	const D3D12_INDEX_BUFFER_VIEW ibView = indexBuffer_->View();
	commandList->IASetIndexBuffer(&ibView);

	// トポロジ設定と描画
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(static_cast<UINT>(indexBuffer_->GetSize() / sizeof(uint32_t)), 1, 0, 0, 0);
}

void SubMesh::ReleaseResource() {
	if (vertexBuffer_) {
		vertexBuffer_.reset();
	}
	if (skinnedVertexBuffer_) {
		skinnedVertexBuffer_.reset();
	}
	if (indexBuffer_) {
		//	indexBuffer_->Release();
		indexBuffer_.reset();
	}
	material_ = nullptr;
}

std::vector<Triangle> SubMesh::GetPolygons() const {
	std::vector<Triangle> polygons;
	if (!indexBuffer_) {
		Console::Print("インデックスバッファが設定されていません", kConTextColorError, Channel::RenderSystem);
		return polygons;
	}
	
	const std::vector<uint32_t>& indices = indexBuffer_->GetIndices();
	
	if (isSkinnedMesh_) {
		if (!skinnedVertexBuffer_) {
			Console::Print("スキニング頂点バッファが設定されていません", kConTextColorError, Channel::RenderSystem);
			return polygons;
		}
		const std::vector<SkinnedVertex>& vertices = skinnedVertexBuffer_->GetVertices();
		for (size_t i = 0; i < indices.size(); i += 3) {
			const SkinnedVertex& v0 = vertices[indices[i]];
			const SkinnedVertex& v1 = vertices[indices[i + 1]];
			const SkinnedVertex& v2 = vertices[indices[i + 2]];
			Vec3 v30 = { v0.position.x, v0.position.y, v0.position.z };
			Vec3 v31 = { v1.position.x, v1.position.y, v1.position.z };
			Vec3 v32 = { v2.position.x, v2.position.y, v2.position.z };

			polygons.emplace_back(v30, v31, v32);
		}
	} else {
		if (!vertexBuffer_) {
			Console::Print("頂点バッファが設定されていません", kConTextColorError, Channel::RenderSystem);
			return polygons;
		}
		const std::vector<Vertex>& vertices = vertexBuffer_->GetVertices();
		for (size_t i = 0; i < indices.size(); i += 3) {
			const Vertex& v0 = vertices[indices[i]];
			const Vertex& v1 = vertices[indices[i + 1]];
			const Vertex& v2 = vertices[indices[i + 2]];
			Vec3 v30 = { v0.position.x, v0.position.y, v0.position.z };
			Vec3 v31 = { v1.position.x, v1.position.y, v1.position.z };
			Vec3 v32 = { v2.position.x, v2.position.y, v2.position.z };

			polygons.emplace_back(v30, v31, v32);
		}
	}
	return polygons;
}

void SubMesh::BuildBVH() {
	// BVHをリセット
	localBVH_.Clear();
	// サブメッシュの全ポリゴンをBVHに登録
	const auto polygons = GetPolygons();
	for (size_t i = 0; i < polygons.size(); ++i) {
		AABB triAABB = FromTriangle(polygons[i]);
		localBVH_.InsertObject(triAABB, static_cast<int>(i));
	}
}

const DynamicBVH& SubMesh::GetBVH() const {
	return localBVH_;
}
