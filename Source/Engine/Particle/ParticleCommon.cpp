#include "ParticleCommon.h"

#include "ParticleObject.h"

#include "../Lib/Console/Console.h"
#include "../Renderer/D3D12.h"
#include "../Renderer/RootSignatureManager.h"
#include "../Lib/Math/Random/Random.h"

void ParticleCommon::Init(D3D12* d3d12, SrvManager* srvManager) {
	d3d12_ = d3d12;
	srvManager_ = srvManager;
	Console::Print("ParticleCommon : ParticleCommonを初期化します。\n", kConsoleColorWait);
	CreateGraphicsPipeline();
	Console::Print("ParticleCommon : ParticleCommonの初期化が完了しました。\n", kConsoleColorCompleted);
}

void ParticleCommon::Shutdown() const {
}

void ParticleCommon::CreateRootSignature() {
	//  RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1, // 数は一つ
		.BaseShaderRegister = 0, // 0から始まる
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offset
	};

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(3);
	// マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0

	// ストラクチャードバッファー
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing; // Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する数

	// テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR, // バイリニアフィルタ
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP, // 0~1の範囲外をリピート
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER, // 比較しない
			.MaxLOD = D3D12_FLOAT32_MAX, // ありったけのMipmapを使う
			.ShaderRegister = 0, // レジスタ番号0を使う
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL // PixelShaderで使う
		},
	};

	// ルートシグネチャを作成
	rootSignatureManager_->CreateRootSignature("ParticleCommon", rootParameters, staticSamplers,
	                                           _countof(staticSamplers));

	if (rootSignatureManager_->Get("ParticleCommon")) {
		Console::Print("ParticleCommon : ルートシグネチャの生成に成功.\n", kConsoleColorCompleted);
	}
}

void ParticleCommon::CreateGraphicsPipeline() {
	CreateRootSignature();

	pipelineState_ = PipelineState(D3D12_CULL_MODE_NONE, D3D12_FILL_MODE_SOLID, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipelineState_.SetInputLayout(Vertex::inputLayout);
	pipelineState_.SetRootSignature(rootSignatureManager_->Get("ParticleCommon"));
	pipelineState_.SetBlendMode(kBlendModeAdd);
	pipelineState_.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO);

	pipelineState_.SetVS(L"./Resources/Shaders/Particle.VS.hlsl");
	pipelineState_.SetPS(L"./Resources/Shaders/Particle.PS.hlsl");
	pipelineState_.Create(d3d12_->GetDevice());


	if (pipelineState_.Get()) {
		Console::Print("ParticleCommon: パイプラインの作成に成功しました。\n");
	}
}

void ParticleCommon::Render() {
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	d3d12_->GetCommandList()->SetGraphicsRootSignature(rootSignatureManager_->Get("ParticleCommon"));
}

D3D12* ParticleCommon::GetD3D12() const { return d3d12_; }

void ParticleCommon::SetDefaultCamera(Camera* newCamera) {
	this->defaultCamera_ = newCamera;
}

Camera* ParticleCommon::GetDefaultCamera() const {
	return defaultCamera_;
}

SrvManager* ParticleCommon::GetSrvManager() const {
	return srvManager_;
}
