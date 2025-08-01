#include <cassert>
#include <engine/public/postprocess/PPVignette.h>
#include <engine/public/renderer/PipelineState.h>
#include <engine/public/renderer/SrvManager.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

PPVignette::PPVignette(ID3D12Device* device, SrvManager* srvMgr)
	: mDevice(device), mSrvMgr(srvMgr) {
	assert(mDevice && mSrvMgr);
	Init();
}

void PPVignette::Init() {
	// RootSig / PSO
	CreateRootSignature();
	CreatePipelineState();

	// SRV スロット確保
	mSrvIndex = mSrvMgr->AllocateForTexture2D();

	// CB (16 byte aligned)
	D3D12_RESOURCE_DESC cbDesc = {};
	cbDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbDesc.Alignment           = 0;
	cbDesc.Width               = (sizeof(VignetteParams) + 255) & ~255;
	cbDesc.Height              = 1;
	cbDesc.DepthOrArraySize    = 1;
	cbDesc.MipLevels           = 1;
	cbDesc.Format              = DXGI_FORMAT_UNKNOWN;
	cbDesc.SampleDesc.Count    = 1;
	cbDesc.SampleDesc.Quality  = 0;
	cbDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	cbDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_UPLOAD};

	HRESULT hr = mDevice->CreateCommittedResource(
		&hp, D3D12_HEAP_FLAG_NONE, &cbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&mParamsCb));
	assert(SUCCEEDED(hr) && "Failed to create constant buffer resource");
}

void PPVignette::Update(float) {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Vignette", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Strength##Vignette", &mParams.vignetteStrength, 0.01f,
		                 0.0f,
		                 100.0f);
		ImGui::DragFloat("Radius##Vignette", &mParams.vignetteRadius, 0.01f,
		                 0.0f,
		                 100.0f);
	}
#endif
}

void PPVignette::Execute(const PostProcessContext& ctx) {
	// ── CB 更新 ──────────────────────────────
	void* p;
	mParamsCb->Map(0, nullptr, &p);
	memcpy(p, &mParams, sizeof(VignetteParams));
	mParamsCb->Unmap(0, nullptr);

	auto* cmd = ctx.commandList;
	cmd->SetPipelineState(mPSO.Get());
	cmd->SetGraphicsRootSignature(mRootSig.Get());

	ID3D12DescriptorHeap* heap = mSrvMgr->GetDescriptorHeap();
	cmd->SetDescriptorHeaps(1, &heap);

	// 出力先 RTV
	cmd->OMSetRenderTargets(1, &ctx.outRtv, FALSE, nullptr);

	// 入力テクスチャを SRV に反映
	mSrvMgr->CreateSRVForTexture2D(
		mSrvIndex, ctx.inputTexture,
		ctx.inputTexture->GetDesc().Format,
		ctx.inputTexture->GetDesc().MipLevels);

	cmd->SetGraphicsRootDescriptorTable(
		0, mSrvMgr->GetGPUDescriptorHandle(mSrvIndex));
	cmd->SetGraphicsRootConstantBufferView(
		1, mParamsCb->GetGPUVirtualAddress());

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

// ───────────────────────────────────────────
void PPVignette::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE1 range = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 1,
		.BaseShaderRegister = 0,
		.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	};

	D3D12_ROOT_PARAMETER1 rp[2] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].DescriptorTable.NumDescriptorRanges = 1;
	rp[0].DescriptorTable.pDescriptorRanges = &range;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rp[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rp[1].Descriptor.ShaderRegister = 0;
	rp[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sam = {
		.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.ShaderRegister = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
	};

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = {
			.NumParameters = 2,
			.pParameters = rp,
			.NumStaticSamplers = 1,
			.pStaticSamplers = &sam,
			.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		}
	};

	Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
	D3D12SerializeVersionedRootSignature(&desc, &sig, &err);
	mDevice->CreateRootSignature(
		0, sig->GetBufferPointer(), sig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSig));
}

void PPVignette::CreatePipelineState() {
	D3D12_DEPTH_STENCIL_DESC ds = {
		.DepthEnable = FALSE, .StencilEnable = FALSE
	};
	PipelineState pso{
		D3D12_CULL_MODE_NONE,
		D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		ds
	};
	pso.SetRootSignature(mRootSig.Get());
	pso.SetInputLayout({});
	pso.SetVertexShader(L"./resources/shaders/Vignette.VS.hlsl");
	pso.SetPixelShader(L"./resources/shaders/Vignette.PS.hlsl");
	pso.SetBlendMode(kBlendModeNone);
	pso.Create(mDevice);
	mPSO = pso.Get();
}
