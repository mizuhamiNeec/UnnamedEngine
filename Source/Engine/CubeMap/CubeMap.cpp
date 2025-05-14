#include "CubeMap.h"

#include "Camera/CameraManager.h"
#include "Components/Camera/CameraComponent.h"
#include "ImGuiManager/ImGuiWidgets.h"
#include "Renderer/ConstantBuffer.h"
#include "Renderer/PipelineState.h"

CubeMap::CubeMap(ID3D12Device* device) : device_(device) {
	Init();
}

void CubeMap::Init() {
	// キューブマップ用の頂点を作る
	constexpr float size = 100.0f;
	// +X
	vertexData_[0] = {{size, size, -size, 1.0f}};
	vertexData_[1] = {{size, size, size, 1.0f}};
	vertexData_[2] = {{size, -size, size, 1.0f}};
	vertexData_[3] = {{size, -size, -size, 1.0f}};
	// -X
	vertexData_[4] = {{-size, size, size, 1.0f}};
	vertexData_[5] = {{-size, size, -size, 1.0f}};
	vertexData_[6] = {{-size, -size, -size, 1.0f}};
	vertexData_[7] = {{-size, -size, size, 1.0f}};
	// +Y
	vertexData_[8]  = {{-size, size, size, 1.0f}};
	vertexData_[9]  = {{size, size, size, 1.0f}};
	vertexData_[10] = {{size, size, -size, 1.0f}};
	vertexData_[11] = {{-size, size, -size, 1.0f}};
	// -Y
	vertexData_[12] = {{-size, -size, -size, 1.0f}};
	vertexData_[13] = {{size, -size, -size, 1.0f}};
	vertexData_[14] = {{size, -size, size, 1.0f}};
	vertexData_[15] = {{-size, -size, size, 1.0f}};
	// +Z
	vertexData_[16] = {{size, size, size, 1.0f}};
	vertexData_[17] = {{-size, size, size, 1.0f}};
	vertexData_[18] = {{-size, -size, size, 1.0f}};
	vertexData_[19] = {{size, -size, size, 1.0f}};
	// -Z
	vertexData_[20] = {{-size, size, -size, 1.0f}};
	vertexData_[21] = {{size, size, -size, 1.0f}};
	vertexData_[22] = {{size, -size, -size, 1.0f}};
	vertexData_[23] = {{-size, -size, -size, 1.0f}};

	indexData_ = {
		0, 1, 2, 0, 2, 3,       // +X
		4, 5, 6, 4, 6, 7,       // -X
		8, 9, 10, 8, 10, 11,    // +Y
		12, 13, 14, 12, 14, 15, // -Y
		16, 17, 18, 16, 18, 19, // +Z
		20, 21, 22, 20, 22, 23  // -Z
	};

	vertexBuffer_ = std::make_unique<VertexBuffer<Vertex>>(
		device_, sizeof(Vertex) * vertexData_.size(), vertexData_.data());
	indexBuffer_ = std::make_unique<IndexBuffer>(
		device_, sizeof(uint32_t) * indexData_.size(), indexData_.data());


	CreateRootSignature();
	CreatePipelineStateObject();

	transformationCB_ = std::make_unique<ConstantBuffer>(
		device_, sizeof(TransformationMatrix), "CubeMap Transformation");

	materialCB_ = std::make_unique<ConstantBuffer>(
		device_, sizeof(Object3D::Material), "CubeMap Material");

	// TransformationMatrixとMaterialの実体をメンバとして保持
	transformationMatrixInstance_ = {};
	materialInstance_             = {};

	// ポインタを実体に向ける
	transformationMatrix_ = &transformationMatrixInstance_;
	material_             = &materialInstance_;

	materialInstance_.color = Vec4::one;
}

void CubeMap::Update([[maybe_unused]] const float deltaTime) {
	const Mat4 world = Mat4::Affine(
		Vec3::one,
		Vec3::zero,
		CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate()
	);

	transformationMatrixInstance_.world = world;
	transformationMatrixInstance_.wvp   =
		transformationMatrixInstance_.world *
		CameraManager::GetActiveCamera()->GetViewMat() *
		CameraManager::GetActiveCamera()->GetProjMat();
}

void CubeMap::Render(ID3D12GraphicsCommandList*    commandList,
                     ShaderResourceViewManager*    shaderSrvManager,
                     const ComPtr<ID3D12Resource>& cubeMapTex) {
	// TransformationMatrixの更新
	memcpy(
		transformationCB_->GetPtr(),
		transformationMatrix_, // → *transformationMatrix_ に修正
		sizeof(TransformationMatrix)
	);

	// マテリアルの更新
	memcpy(
		materialCB_->GetPtr(),
		material_, // → *material_ に修正
		sizeof(Object3D::Material)
	);

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());

	ID3D12DescriptorHeap* heaps[] = {
		ShaderResourceViewManager::GetDescriptorHeap().Get()
	};
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	// SRV登録
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // テクスチャのフォーマットに合わせる
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = UINT_MAX;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	DescriptorHandles handles = shaderSrvManager->RegisterShaderResourceView(
		cubeMapTex.Get(), srvDesc);

	// ルートパラメータでSRVとCBVをバインド
	commandList->SetGraphicsRootDescriptorTable(0, handles.gpuHandle); // t0
	commandList->SetGraphicsRootConstantBufferView(
		1, transformationCB_->GetAddress()); // b0: TransformationMatrix
	commandList->SetGraphicsRootConstantBufferView(
		2, materialCB_->GetAddress()); // b1: Material

	// 頂点・インデックスバッファのセット
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	D3D12_INDEX_BUFFER_VIEW  ibView = indexBuffer_->View();
	commandList->IASetVertexBuffers(0, 1, &vbView);
	commandList->IASetIndexBuffer(&ibView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void CubeMap::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE1 range           = {};
	range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors                    = 1;
	range.BaseShaderRegister                = 0;
	range.RegisterSpace                     = 0;
	range.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

	D3D12_ROOT_PARAMETER1 rootParameters[3] = {};
	// テクスチャ用のSRVを登録
	rootParameters[0].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &range;

	// 頂点シェーダ用CBV（b0）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0
	rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	// ピクセルシェーダ用CBV（b1）
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 1; // b1
	rootParameters[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.MipLODBias = 0.0f;
	staticSampler.MaxAnisotropy = 1;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	staticSampler.MinLOD = 0.0f;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0; // s0
	staticSampler.RegisterSpace = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
	desc.Version                             = D3D_ROOT_SIGNATURE_VERSION_1_1;
	desc.Desc_1_1.NumParameters              = 3;
	desc.Desc_1_1.pParameters                = rootParameters;
	desc.Desc_1_1.NumStaticSamplers          = 1;
	desc.Desc_1_1.pStaticSamplers            = &staticSampler;
	desc.Desc_1_1.Flags                      =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeVersionedRootSignature(
		&desc, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = device_->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	assert(SUCCEEDED(hr));
}

void CubeMap::CreatePipelineStateObject() {
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
		.DepthEnable = true, // 比較はするのでDepth自体は有効
		.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
		// 全ピクセルがz=1に出力されるので、わざわざ書き込む必要がない
		.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL, // 今までと同様に比較
	};

	PipelineState pso{
		D3D12_CULL_MODE_NONE,
		D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		depthStencilDesc
	};

	pso.SetInputLayout(Vertex::inputLayout);
	pso.SetRootSignature(rootSignature.Get());
	pso.SetVS(L"./Resources/Shaders/Skybox.VS.hlsl");
	pso.SetPS(L"./Resources/Shaders/Skybox.PS.hlsl");
	pso.SetBlendMode(kBlendModeNone);

	pso.Create(device_);

	pipelineState = pso.Get();
}
