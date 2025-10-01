#include <pch.h>

//-----------------------------------------------------------------------------

#include <cassert>

#include <engine/OldConsole/Console.h>
#include <engine/postprocess/PPBloom.h>
#include <engine/renderer/PipelineState.h>
#include <engine/renderer/SrvManager.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

PPBloom::PPBloom(ID3D12Device* device, SrvManager* srvManager)
	: mDevice(device), mSrvManager(srvManager) {
	assert(mDevice && mSrvManager);
	Init();
}

void PPBloom::Init() {
	CreateRootSignature();
	CreatePipelineState();

	mSrvIndex = mSrvManager->AllocateForTexture2D();

	D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_UPLOAD};
	D3D12_RESOURCE_DESC   desc      = {};
	desc.Dimension                  = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width                      = (sizeof(BloomParams) + 255) & ~255;
	desc.Height                     = 1;
	desc.DepthOrArraySize           = 1;
	desc.MipLevels                  = 1;
	desc.Format                     = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count           = 1;
	desc.Layout                     = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = mDevice->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&mBloomCb));
	UASSERT(SUCCEEDED(hr));
}

void PPBloom::Update(float) {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Bloom Settings",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Strength##Bloom", &mBloomParams.bloomStrength, 0.01f,
		                 0.0f,
		                 10.0f);
		ImGui::DragFloat("Threshold##Bloom", &mBloomParams.bloomThreshold,
		                 0.01f, 0.0f,
		                 10.0f);
	}
#endif
}

void PPBloom::Execute(const PostProcessContext& context) {
	auto* cmd = context.commandList;

	void*   pData = nullptr;
	HRESULT hr    = mBloomCb->Map(0, nullptr, &pData);
	UASSERT(SUCCEEDED(hr) && pData != nullptr &&
		"Failed to map Bloom constant buffer"
	);
	memcpy(pData, &mBloomParams, sizeof(BloomParams));
	mBloomCb->Unmap(0, nullptr);

	cmd->SetPipelineState(mPipelineState.Get());
	cmd->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12DescriptorHeap* heap = mSrvManager->GetDescriptorHeap();
	cmd->SetDescriptorHeaps(1, &heap);

	cmd->OMSetRenderTargets(1, &context.outRtv, FALSE, nullptr);
	mSrvManager->CreateSRVForTexture2D(
		mSrvIndex, context.inputTexture,
		context.inputTexture->GetDesc().Format,
		context.inputTexture->GetDesc().MipLevels
	);

	D3D12_GPU_DESCRIPTOR_HANDLE srv = mSrvManager->GetGPUDescriptorHandle(
		mSrvIndex);
	cmd->SetGraphicsRootDescriptorTable(0, srv);
	cmd->SetGraphicsRootConstantBufferView(
		1, mBloomCb->GetGPUVirtualAddress());

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void PPBloom::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE1 range = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 1,
		.BaseShaderRegister = 0,
		.RegisterSpace = 0,
		.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	};

	D3D12_ROOT_PARAMETER1 rootParams[2] = {};
	rootParams[0].ParameterType         =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &range;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootParams[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 0;
	rootParams[1].Descriptor.RegisterSpace  = 0;
	rootParams[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sampler = {
		.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.ShaderRegister = 0,
		.RegisterSpace = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
	};

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = {
			.NumParameters = 2,
			.pParameters = rootParams,
			.NumStaticSamplers = 1,
			.pStaticSamplers = &sampler,
			.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		}
	};

	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	HRESULT                          hr =
		D3D12SerializeVersionedRootSignature(&desc, &sigBlob, &errBlob);
	assert(SUCCEEDED(hr));
	hr = mDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(),
	                                  sigBlob->GetBufferSize(),
	                                  IID_PPV_ARGS(&mRootSignature));
	assert(SUCCEEDED(hr));
}

void PPBloom::CreatePipelineState() {
	D3D12_DEPTH_STENCIL_DESC depth = {
		.DepthEnable = FALSE,
		.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
		.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
		.StencilEnable = FALSE
	};

	PipelineState pso{
		D3D12_CULL_MODE_NONE,
		D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		depth
	};

	pso.SetRootSignature(mRootSignature.Get());
	pso.SetInputLayout({});
	pso.SetVertexShader(L"./resources/shaders/Bloom.VS.hlsl");
	pso.SetPixelShader(L"./resources/shaders/Bloom.PS.hlsl");
	pso.SetBlendMode(kBlendModeNone);

	pso.Create(mDevice);
	mPipelineState = pso.Get();
}
