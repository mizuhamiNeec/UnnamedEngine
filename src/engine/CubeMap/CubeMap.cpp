#include <engine/CubeMap/CubeMap.h>

#include "engine/Camera/CameraManager.h"
#include "engine/Components/Camera/CameraComponent.h"
#include "engine/renderer/ConstantBuffer.h"
#include "engine/renderer/PipelineState.h"
#include "engine/renderer/SrvManager.h"
#include "engine/TextureManager/TexManager.h"

CubeMap::CubeMap(
	ID3D12Device*          device,
	SrvManager*            srvManager,
	const std::string_view path
) : mVertexData({}), mDevice(device), mSrvManager(srvManager) {
	mTexturePath = path;
	Init();
}

void CubeMap::Init() {
	// キューブマップ用の頂点を作る
	constexpr float size = 100.0f;
	// +X
	mVertexData[0] = {.position = {size, size, -size, 1.0f}};
	mVertexData[1] = {.position = {size, size, size, 1.0f}};
	mVertexData[2] = {.position = {size, -size, size, 1.0f}};
	mVertexData[3] = {.position = {size, -size, -size, 1.0f}};
	// -X
	mVertexData[4] = {.position = {-size, size, size, 1.0f}};
	mVertexData[5] = {.position = {-size, size, -size, 1.0f}};
	mVertexData[6] = {.position = {-size, -size, -size, 1.0f}};
	mVertexData[7] = {.position = {-size, -size, size, 1.0f}};
	// +Y
	mVertexData[8]  = {.position = {-size, size, size, 1.0f}};
	mVertexData[9]  = {.position = {size, size, size, 1.0f}};
	mVertexData[10] = {.position = {size, size, -size, 1.0f}};
	mVertexData[11] = {.position = {-size, size, -size, 1.0f}};
	// -Y
	mVertexData[12] = {.position = {-size, -size, -size, 1.0f}};
	mVertexData[13] = {.position = {size, -size, -size, 1.0f}};
	mVertexData[14] = {.position = {size, -size, size, 1.0f}};
	mVertexData[15] = {.position = {-size, -size, size, 1.0f}};
	// +Z
	mVertexData[16] = {.position = {size, size, size, 1.0f}};
	mVertexData[17] = {.position = {-size, size, size, 1.0f}};
	mVertexData[18] = {.position = {-size, -size, size, 1.0f}};
	mVertexData[19] = {.position = {size, -size, size, 1.0f}};
	// -Z
	mVertexData[20] = {.position = {-size, size, -size, 1.0f}};
	mVertexData[21] = {.position = {size, size, -size, 1.0f}};
	mVertexData[22] = {.position = {size, -size, -size, 1.0f}};
	mVertexData[23] = {.position = {-size, -size, -size, 1.0f}};

	mIndexData = {
		0, 1, 2, 0, 2, 3,       // +X
		4, 5, 6, 4, 6, 7,       // -X
		8, 9, 10, 8, 10, 11,    // +Y
		12, 13, 14, 12, 14, 15, // -Y
		16, 17, 18, 16, 18, 19, // +Z
		20, 21, 22, 20, 22, 23  // -Z
	};

	mVertexBuffer = std::make_unique<VertexBuffer<Vertex>>(
		mDevice, sizeof(Vertex) * mVertexData.size(), mVertexData.data());
	mIndexBuffer = std::make_unique<IndexBuffer>(
		mDevice, sizeof(uint32_t) * mIndexData.size(), mIndexData.data());


	CreateRootSignature();
	CreatePipelineStateObject();

	mTransformationCb = std::make_unique<ConstantBuffer>(
		mDevice, sizeof(TransformationMatrix), "CubeMap Transformation");

	mMaterialCb = std::make_unique<ConstantBuffer>(
		mDevice, sizeof(Object3D::Material), "CubeMap Material");

	// TransformationMatrixとMaterialの実体をメンバとして保持
	mTransformationMatrixInstance = {};
	mMaterialInstance             = {};

	// ポインタを実体に向ける
	mTransformationMatrix = &mTransformationMatrixInstance;
	mMaterial             = &mMaterialInstance;

	mMaterialInstance.color = Vec4::one;
}

void CubeMap::Update([[maybe_unused]] const float deltaTime) {
	const Mat4 world = Mat4::Affine(
		Vec3::one,
		Vec3::zero,
		CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate()
	);

	mTransformationMatrixInstance.world = world;
	mTransformationMatrixInstance.wvp   =
		mTransformationMatrixInstance.world *
		CameraManager::GetActiveCamera()->GetViewMat() *
		CameraManager::GetActiveCamera()->GetProjMat();
}

void CubeMap::Render(ID3D12GraphicsCommandList* commandList) const {
	// TransformationMatrixの更新
	memcpy(
		mTransformationCb->GetPtr(),
		mTransformationMatrix, // → *transformationMatrix_ に修正
		sizeof(TransformationMatrix)
	);

	// マテリアルの更新
	memcpy(
		mMaterialCb->GetPtr(),
		mMaterial, // → *material_ に修正
		sizeof(Object3D::Material)
	);

	commandList->SetPipelineState(mPipelineState.Get());
	commandList->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12DescriptorHeap* heaps[] = {
		mSrvManager->GetDescriptorHeap(),
	};
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	// ルートパラメータでSRVとCBVをバインド
	commandList->SetGraphicsRootDescriptorTable(
		0, TexManager::GetInstance()->GetSrvHandleGPU(mTexturePath)
	); // t0
	commandList->SetGraphicsRootConstantBufferView(
		1, mTransformationCb->GetAddress()); // b0: TransformationMatrix
	commandList->SetGraphicsRootConstantBufferView(
		2, mMaterialCb->GetAddress()); // b1: Material

	// 頂点・インデックスバッファのセット
	const D3D12_VERTEX_BUFFER_VIEW vbView = mVertexBuffer->View();
	const D3D12_INDEX_BUFFER_VIEW  ibView = mIndexBuffer->View();
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

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeVersionedRootSignature(
		&desc, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = mDevice->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));

	assert(SUCCEEDED(hr));
}

void CubeMap::CreatePipelineStateObject() {
	const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
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
	pso.SetRootSignature(mRootSignature.Get());
	pso.SetVertexShader(L"./content/core/shaders/Skybox.VS.hlsl");
	pso.SetPixelShader(L"./content/core/shaders/Skybox.PS.hlsl");
	pso.SetBlendMode(kBlendModeNone);

	pso.Create(mDevice);

	mPipelineState = pso.Get();
}
