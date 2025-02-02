#pragma once
#include <string>

#include <Renderer/VertexBuffer.h>
#include <Renderer/IndexBuffer.h>

#include <ResourceSystem/Material/Material.h>

#include <Lib/Structs/Structs.h>

#include <Physics/Physics.h>

class SubMesh {
public:
	SubMesh(const ComPtr<ID3D12Device>& device, std::string name);
	~SubMesh();

	void SetVertexBuffer(const std::vector<Vertex>& vertices);
	void SetIndexBuffer(const std::vector<uint32_t>& indices);

	[[nodiscard]] Material* GetMaterial() const;
	void SetMaterial(Material* material);

	std::string& GetName();

	void Render(ID3D12GraphicsCommandList* commandList) const;

	void ReleaseResource();
	std::vector<Triangle> GetPolygons() const;

private:
	std::string name_;
	ComPtr<ID3D12Device> device_;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_;
	std::unique_ptr<IndexBuffer> indexBuffer_;
	Material* material_ = nullptr;
};
