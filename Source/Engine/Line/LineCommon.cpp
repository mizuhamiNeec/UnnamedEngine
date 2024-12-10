#include "LineCommon.h"

#include "Line.h"
#include "../Lib/Console/Console.h"
#include "../EntityComponentSystem/Entity/Camera/Camera.h"

//-----------------------------------------------------------------------------
// Purpose : LineCommonを初期化します
//-----------------------------------------------------------------------------
void LineCommon::Init(D3D12* d3d12) {
	this->d3d12_ = d3d12;
	Console::Print("LineCommon : Lineを初期化します。\n");
	CreateGraphicsPipeline();
	Console::Print("LineCommon : Lineの初期化が完了しました。\n");
}

//-----------------------------------------------------------------------------
// Purpose : LineCommonをシャットダウンします
//-----------------------------------------------------------------------------
void LineCommon::Shutdown() const {
}

//-----------------------------------------------------------------------------
// Purpose : ルートシグネチャを作成します
//-----------------------------------------------------------------------------
void LineCommon::CreateRootSignature() {
	// RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(d3d12_->GetDevice());

	// D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	// descriptorRange[0] = {
	// 	.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
	// 	.NumDescriptors = 1, // 数は一つ
	// 	.BaseShaderRegister = 0, // 0から始まる
	// 	.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	// };

	// ルートパラメーターを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(1);
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド。b0の0と一致する。もしb11と紐づけたいなら11となる

	// ルートシグネチャを作成
	rootSignatureManager_->CreateRootSignature(
		"Line",
		rootParameters,
		nullptr, // 今回サンプラーは使用しない
		0
	);

	if (rootSignatureManager_->Get("Line")) {
		Console::Print("LineCommon : ルートシグネチャの生成に成功.\n", kConsoleColorCompleted);
	}
}

//-----------------------------------------------------------------------------
// Purpose : パイプラインを作成します
//-----------------------------------------------------------------------------
void LineCommon::CreateGraphicsPipeline() {
	CreateRootSignature();

	// パイプラインステートを作成
	pipelineState_ = PipelineState(D3D12_CULL_MODE_BACK, D3D12_FILL_MODE_SOLID, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	pipelineState_.SetInputLayout(LineVertex::inputLayout);
	pipelineState_.SetRootSignature(rootSignatureManager_->Get("Line"));

	pipelineState_.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ALL);
	pipelineState_.SetBlendMode(kBlendModeNone);

	// シェーダーのファイルパスを設定
	pipelineState_.SetVS(L"./Resources/Shaders/Line.VS.hlsl");
	pipelineState_.SetPS(L"./Resources/Shaders/Line.PS.hlsl");
	pipelineState_.Create(d3d12_->GetDevice());

	if (pipelineState_.Get()) {
		Console::Print("LineCommon : パイプラインステートの作成に成功.\n", kConsoleColorCompleted);
	}
}

//-----------------------------------------------------------------------------
// Purpose : 共通描画設定
//-----------------------------------------------------------------------------
void LineCommon::Render() const {
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	d3d12_->GetCommandList()->SetGraphicsRootSignature(rootSignatureManager_->Get("Line"));
	d3d12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

//-----------------------------------------------------------------------------
// Purpose : レンダラを取得します
//-----------------------------------------------------------------------------
D3D12* LineCommon::GetD3D12() const {
	return d3d12_;
}

//-----------------------------------------------------------------------------
// Purpose : デフォルトのカメラを設定します
//-----------------------------------------------------------------------------
void LineCommon::SetDefaultCamera(Camera* camera) {
	this->defaultCamera_ = camera;
}

//-----------------------------------------------------------------------------
// Purpose : デフォルトのカメラを取得します
//-----------------------------------------------------------------------------
Camera* LineCommon::GetDefaultCamera() const {
	return this->defaultCamera_;
}
