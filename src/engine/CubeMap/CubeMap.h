#pragma once
#include <array>

#include <engine/Object3D/Object3D.h>
#include <engine/renderer/IndexBuffer.h>
#include <engine/renderer/Structs.h>
#include <engine/renderer/VertexBuffer.h>

class SrvManager;

class CubeMap {
public:
	CubeMap(
		ID3D12Device*    device,
		SrvManager*      srvManager,
		std::string_view path
	);

	void Update(const float deltaTime);
	void Render(
		ID3D12GraphicsCommandList* commandList
	) const;

private:
	void Init();
	void CreateRootSignature();
	void CreatePipelineStateObject();

	std::array<Vertex, 24>                mVertexData;
	std::array<uint32_t, 36>              mIndexData;
	std::unique_ptr<VertexBuffer<Vertex>> mVertexBuffer;
	std::unique_ptr<IndexBuffer>          mIndexBuffer;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
	ID3D12Device*                               mDevice;

	std::unique_ptr<ConstantBuffer> mTransformationCb;
	std::unique_ptr<ConstantBuffer> mMaterialCb;

	TransformationMatrix* mTransformationMatrix;
	Object3D::Material*   mMaterial;

	TransformationMatrix mTransformationMatrixInstance;
	Object3D::Material   mMaterialInstance;
	std::string          mTexturePath;

	SrvManager* mSrvManager = nullptr;
};
