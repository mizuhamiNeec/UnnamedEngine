#pragma once
#include <array>

#include <Lib/Structs/Structs.h>
#include <Object3D/Object3D.h>
#include <Renderer/D3D12.h>
#include <Renderer/IndexBuffer.h>

class CubeMap {
public:
	CubeMap(ID3D12Device* device);

	void Update(const float deltaTime);
	void Render(
		ID3D12GraphicsCommandList* commandList, ShaderResourceViewManager*
		shaderSrvManager, const ComPtr<ID3D12Resource>& cubeMapTex
	);

private:
	void Init();
	void CreateRootSignature();
	void CreatePipelineStateObject();

	std::array<Vertex, 24>                vertexData_;
	std::array<uint32_t, 36>              indexData_;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_;
	std::unique_ptr<IndexBuffer>          indexBuffer_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12Device*                               device_;

	std::unique_ptr<ConstantBuffer> transformationCB_;
	std::unique_ptr<ConstantBuffer> materialCB_;

	TransformationMatrix* transformationMatrix_;
	Object3D::Material*   material_;

	TransformationMatrix transformationMatrixInstance_;
	Object3D::Material   materialInstance_;
};
