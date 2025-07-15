#include "LineCommon.h"

#include "../SubSystem/Console/Console.h"
#include "Line.h"
#include "Camera/CameraManager.h"

//-----------------------------------------------------------------------------
// Purpose : LineCommonを初期化します
//-----------------------------------------------------------------------------
void LineCommon::Init(D3D12* d3d12) {
	this->mRenderer = d3d12;
	Console::Print("LineCommon : Lineを初期化します。\n", kConTextColorWait,
	               Channel::Engine);
	CreateGraphicsPipeline();
	Console::Print("LineCommon : Lineの初期化が完了しました。\n", kConTextColorCompleted,
	               Channel::Engine);
}

//-----------------------------------------------------------------------------
// Purpose : LineCommonをシャットダウンします
//-----------------------------------------------------------------------------
void LineCommon::Shutdown() const {
	mRootSignatureManager->Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose : ルートシグネチャを作成します
//-----------------------------------------------------------------------------
void LineCommon::CreateRootSignature() {
	// RootSignatureManagerのインスタンスを作成
	mRootSignatureManager = std::make_unique<RootSignatureManager>(
		mRenderer->GetDevice());

	// ルートパラメーターを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(1);
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;
	// レジスタ番号0とバインド。b0の0と一致する。もしb11と紐づけたいなら11となる

	// ルートシグネチャを作成
	mRootSignatureManager->CreateRootSignature(
		"Line",
		rootParameters,
		nullptr, // 今回サンプラーは使用しない
		0
	);

	if (mRootSignatureManager->Get("Line")) {
		Console::Print("LineCommon : ルートシグネチャの生成に成功.\n", kConTextColorCompleted,
		               Channel::Engine);
	}
}

//-----------------------------------------------------------------------------
// Purpose : パイプラインを作成します
//-----------------------------------------------------------------------------
void LineCommon::CreateGraphicsPipeline() {
	CreateRootSignature();

	// パイプラインステートを作成
	mPipelineState = PipelineState(D3D12_CULL_MODE_BACK, D3D12_FILL_MODE_SOLID,
	                               D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	mPipelineState.SetInputLayout(LineVertex::inputLayout);
	mPipelineState.SetRootSignature(mRootSignatureManager->Get("Line"));

	mPipelineState.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ALL);
	mPipelineState.SetBlendMode(kBlendModeNone);

	// シェーダーのファイルパスを設定
	mPipelineState.SetVertexShader(L"./Resources/Shaders/Line.VS.hlsl");
	mPipelineState.SetPixelShader(L"./Resources/Shaders/Line.PS.hlsl");
	mPipelineState.Create(mRenderer->GetDevice());

	if (mPipelineState.Get()) {
		Console::Print("LineCommon : パイプラインステートの作成に成功.\n",
		               kConTextColorCompleted, Channel::Engine);
	}
}

//-----------------------------------------------------------------------------
// Purpose : 共通描画設定
//-----------------------------------------------------------------------------
void LineCommon::Render() const {
	mRenderer->GetCommandList()->SetPipelineState(mPipelineState.Get());
	mRenderer->GetCommandList()->SetGraphicsRootSignature(
		mRootSignatureManager->Get("Line"));
	mRenderer->GetCommandList()->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

//-----------------------------------------------------------------------------
// Purpose : レンダラを取得します
//-----------------------------------------------------------------------------
D3D12* LineCommon::GetRenderer() const {
	return mRenderer;
}
