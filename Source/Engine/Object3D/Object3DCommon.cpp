#include "Object3DCommon.h"

#include "../Lib/Console/Console.h"
#include "../Lib/Structs/Structs.h"
#include "../Renderer/RootSignatureManager.h"

#include "../Renderer/SrvManager.h"

//-----------------------------------------------------------------------------
// Purpose : Object3DCommonを初期化します
//-----------------------------------------------------------------------------
void Object3DCommon::Init(D3D12* d3d12) {
	this->d3d12_ = d3d12;
	Console::Print("Object3DCommon : Object3dを初期化します。\n", kConsoleColorWait);
	CreateGraphicsPipeline();
	Console::Print("Object3DCommon : Object3dの初期化が完了しました。\n", kConsoleColorCompleted);
}

//-----------------------------------------------------------------------------
// Purpose : Object3DCommonをシャットダウンします
//-----------------------------------------------------------------------------
void Object3DCommon::Shutdown() const {
}

//-----------------------------------------------------------------------------
// Purpose : ルートシグネチャを作成します
//-----------------------------------------------------------------------------
void Object3DCommon::CreateRootSignature() {
	// RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1, // 数は一つ
		.BaseShaderRegister = 0, // 0から始まる
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offset
	};

	// ルートパラメータを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(7);
	// ピクセルシェーダー : マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う。b0のbと一致する
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド。b0の0と一致する。もしb11と紐づけたいなら11となる

	// 頂点シェーダー : 変換行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// ピクセルシェーダー : ???
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// ピクセルシェーダー : 指向性ライト
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	// 頂点シェーダー : カメラ
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[4].Descriptor.ShaderRegister = 2; // レジスタ番号2を使う

	// ピクセルシェーダー : ポイントライト
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[5].Descriptor.ShaderRegister = 3; // レジスタ番号3を使う

	// ピクセルシェーダー : スポットライト
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[6].Descriptor.ShaderRegister = 4; // レジスタ番号3を使う

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_ANISOTROPIC, // バイリニアフィルタ
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
	rootSignatureManager_->CreateRootSignature("Object3d", rootParameters, staticSamplers, _countof(staticSamplers));

	if (rootSignatureManager_->Get("Object3d")) {
		Console::Print("Object3DCommon : ルートシグネチャの生成に成功.\n", kConsoleColorCompleted);
	}
}

//-----------------------------------------------------------------------------
// Purpose : パイプラインを作成します
//-----------------------------------------------------------------------------
void Object3DCommon::CreateGraphicsPipeline() {
	CreateRootSignature();

	// パイプラインステートを作成
	pipelineState_ = PipelineState(D3D12_CULL_MODE_BACK, D3D12_FILL_MODE_SOLID, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipelineState_.SetInputLayout(Vertex::inputLayout);
	pipelineState_.SetRootSignature(rootSignatureManager_->Get("Object3d"));

	pipelineState_.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ALL);
	pipelineState_.SetBlendMode(kBlendModeNormal);

	// シェーダーのファイルパスを設定
	pipelineState_.SetVS(L"./Resources/Shaders/Object3d.VS.hlsl");
	pipelineState_.SetPS(L"./Resources/Shaders/Object3d.PS.hlsl");
	pipelineState_.Create(d3d12_->GetDevice());

	if (pipelineState_.Get()) {
		Console::Print("Object3DCommon : パイプラインステートの作成に成功.\n", kConsoleColorCompleted);
	}
}

//-----------------------------------------------------------------------------
// Purpose : 共通描画設定
//-----------------------------------------------------------------------------
void Object3DCommon::Render() const {
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	d3d12_->GetCommandList()->SetGraphicsRootSignature(rootSignatureManager_->Get("Object3d"));
	d3d12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

D3D12* Object3DCommon::GetD3D12() const {
	return d3d12_;
}

void Object3DCommon::SetDefaultCamera(Camera* camera) {
	this->defaultCamera_ = camera;
}

Camera* Object3DCommon::GetDefaultCamera() const {
	return defaultCamera_;
}
