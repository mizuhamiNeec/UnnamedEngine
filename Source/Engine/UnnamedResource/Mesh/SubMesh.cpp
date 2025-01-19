#include "SubMesh.h"

#include "Lib/Console/Console.h"

SubMesh::SubMesh(const ComPtr<ID3D12Device>& device, std::string name) :
	name_(std::move(name)),
	device_(device) {
}

SubMesh::~SubMesh() {
}

void SubMesh::SetVertexBuffer(const std::vector<Vertex>& vertices) {
	size_t size = vertices.size() * sizeof(Vertex);
	vertexBuffer_ = std::make_unique<VertexBuffer<Vertex>>(device_, size, vertices.data());
}

void SubMesh::SetIndexBuffer(const std::vector<uint32_t>& indices) {
	size_t size = indices.size() * sizeof(uint32_t);
	indexBuffer_ = std::make_unique<IndexBuffer>(device_, size, indices.data());
}

Material* SubMesh::GetMaterial() const { return material_; }

void SubMesh::SetMaterial(Material* material) { material_ = material; }

void SubMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	// 頂点バッファとインデックスバッファを設定
	if (!vertexBuffer_ || !indexBuffer_) {
		Console::Print("頂点バッファまたはインデックスバッファが設定されていません", kConsoleColorError, Channel::RenderSystem);
		return;
	}
	const D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	commandList->IASetVertexBuffers(0, 1, &vbView);
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
	if (indexBuffer_) {
	//	indexBuffer_->Release();
		indexBuffer_.reset();
	}
	material_ = nullptr;
}
