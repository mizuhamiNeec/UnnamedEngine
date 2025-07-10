#include "CopyImagePass.h"

#include <cassert>
#include <format>

#include "SrvManager.h"
#include <SubSystem/Console/Console.h>

#include "Lib/Structs/Structs.h"

#include "Renderer/PipelineState.h"

CopyImagePass::CopyImagePass(
	ID3D12Device* device,
	SrvManager*   srvManager
) : srvManager_(srvManager),
    device_(device) {
	// SrvManagerの有効性をチェック
	assert(srvManager_ != nullptr && "SrvManager is null");
	assert(device_ != nullptr && "Device is null");
	
	Init();
}

CopyImagePass::~CopyImagePass() = default;

void CopyImagePass::Init() {
	CreateRootSignature();
	CreatePipelineState();

	// SrvManagerが有効であることを確認してからSRVインデックスを確保
	assert(srvManager_ != nullptr && "SrvManager is null in Init()");
	srvIndex_ = srvManager_->AllocateForTexture(); // テクスチャ用SRVのインデックスを確保

	D3D12_HEAP_PROPERTIES props = {};
	props.Type                  = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = (sizeof(PostProcessParams) + 255) & ~255; // 256バイトアライメント
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device_->CreateCommittedResource(
		&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&postProcessParamsCB_)
	);
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr));
	}
}

void CopyImagePass::Update([[maybe_unused]] const float deltaTime) {
#ifdef _DEBUG
	ImGui::Begin("PostProcess");
	if (ImGui::CollapsingHeader("Simple Bloom")) {
		ImGui::DragFloat("Bloom Strength", &postProcessParams_.bloomStrength,
		                 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Bloom Threshold", &postProcessParams_.bloomThreshold,
		                 0.01f, 0.0f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Vignette")) {
		ImGui::DragFloat("Vignette Strength",
		                 &postProcessParams_.vignetteStrength, 0.01f, 0.0f,
		                 10.0f);
		ImGui::DragFloat("Vignette Radius", &postProcessParams_.vignetteRadius,
		                 0.01f, 0.0f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Chromatic Aberration")) {
		ImGui::DragFloat("Chromatic Aberration Strength",
		                 &postProcessParams_.chromaticAberration, 0.01f, 0.0f,
		                 10.0f);
	}

	ImGui::End();
#endif
}

void CopyImagePass::Execute(
	ID3D12GraphicsCommandList*  commandList,
	ID3D12Resource*             srcTexture,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv
) {
	// SrvManagerの有効性を再チェック
	assert(srvManager_ != nullptr && "SrvManager is null in Execute");
	
	void*   pData = nullptr;
	HRESULT hr    = postProcessParamsCB_->Map(0, nullptr, &pData);
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr));
		return;
	}
	memcpy(pData, &postProcessParams_, sizeof(PostProcessParams));
	postProcessParamsCB_->Unmap(0, nullptr);

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());

	// ディスクリプターヒープの有効性をチェック
	ID3D12DescriptorHeap* descriptorHeap = srvManager_->GetDescriptorHeap();
	if (descriptorHeap != nullptr) {
		ID3D12DescriptorHeap* heaps[] = {
			descriptorHeap
		};
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	} else {
		// ディスクリプターヒープがnullの場合、エラーログを出力
		assert(false && "SrvManager's descriptor heap is null in Execute");
		return;
	}

	// RTVセット
	commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

	// 2. SRV登録＆ハンドル取得
	// D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	// srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// srvDesc.Format = srcTexture->GetDesc().Format;
	// srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// srvDesc.Texture2D.MostDetailedMip = 0;
	// srvDesc.Texture2D.MipLevels = srcTexture->GetDesc().MipLevels;
	// srvDesc.Texture2D.PlaneSlice = 0;
	// srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// フォーマット情報のデバッグ出力
	const auto srcFormat = srcTexture->GetDesc().Format;
	
	// SRV作成
	srvManager_->CreateSRVForTexture2D(
		srvIndex_,
		srcTexture,
		srcFormat,
		srcTexture->GetDesc().MipLevels
	);

	// GPUハンドルの取得とデバッグ情報出力
	const auto gpuHandle = srvManager_->GetGPUDescriptorHandle(srvIndex_);
	
	static bool loggedOnce = false;
	if (!loggedOnce) {
		Console::Print(std::format("CopyImagePass: srvIdx={}, format={}, handle=0x{:x}\n", 
			srvIndex_, static_cast<int>(srcFormat), gpuHandle.ptr), kConTextColorGray);
		loggedOnce = true;
	}

	// 3. SRVをルートテーブルにバインド
	commandList->SetGraphicsRootDescriptorTable(
		0, gpuHandle); // SRV
	//
	commandList->SetGraphicsRootConstantBufferView(
		1, postProcessParamsCB_->GetGPUVirtualAddress()); // CBV

	// フルスクリーン三角形の頂点バッファ: ここは外部でセットでも可
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// フルスクリーン三角形描画 (3頂点)
	commandList->DrawInstanced(3, 1, 0, 0);
}

void CopyImagePass::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE1 range           = {};
	range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors                    = 1;
	range.BaseShaderRegister                = 0;
	range.RegisterSpace                     = 0;
	range.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

	D3D12_ROOT_PARAMETER1 rootParameters[2] = {};
	rootParameters[0].ParameterType         =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &range;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0
	rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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
	desc.Desc_1_1.NumParameters              = 2;
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
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	assert(SUCCEEDED(hr));
}

void CopyImagePass::CreatePipelineState() {
	// 頂点には何もデータを入力しない
	D3D12_INPUT_LAYOUT_DESC inputLayout = {};
	inputLayout.pInputElementDescs      = nullptr;
	inputLayout.NumElements             = 0;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
		.DepthEnable = false, // 全画面に対してなにか処理を施したいだけだから、比較も書き込みも必要ないのでDepth自体不要
		.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO, // 書き込まない
		.DepthFunc = D3D12_COMPARISON_FUNC_LESS, // 比較関数はLess
		.StencilEnable = FALSE
	};

	PipelineState pso{
		D3D12_CULL_MODE_NONE,
		D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		depthStencilDesc
	};
	pso.SetInputLayout(inputLayout);
	pso.SetRootSignature(rootSignature.Get());
	pso.SetVS(L"./Resources/Shaders/CopyImage.VS.hlsl");
	pso.SetPS(L"./Resources/Shaders/CopyImage.PS.hlsl");
	pso.SetBlendMode(kBlendModeNone);

	pso.Create(device_);

	pipelineState = pso.Get();
}
