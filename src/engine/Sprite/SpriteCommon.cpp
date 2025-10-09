#include <d3d12.h>
#include <memory>
#include <vector>

#include <engine/OldConsole/Console.h>
#include <engine/renderer/D3D12.h>
#include <engine/Sprite/Sprite.h>
#include <engine/Sprite/SpriteCommon.h>

//-----------------------------------------------------------------------------
// Purpose: SpriteCommonを初期化します
//-----------------------------------------------------------------------------
void SpriteCommon::Init(D3D12* d3d12) {
	d3d12_ = d3d12;
	Console::Print("SpriteCommon : SpriteCommonを初期化します。\n", kConTextColorWait,
	               Channel::Engine);
	CreateGraphicsPipeline();
	Console::Print("SpriteCommon : SpriteCommonの初期化が完了しました。\n",
	               kConTextColorCompleted, Channel::Engine);
}

//-----------------------------------------------------------------------------
// Purpose: SpriteCommonをシャットダウンします
//-----------------------------------------------------------------------------
void SpriteCommon::Shutdown() const {
	rootSignatureManager_->Shutdown();
}

void SpriteCommon::CreateRootSignature() {
	//  RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(
		d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1,                          // 数は一つ
		.BaseShaderRegister = 0,                      // 0から始まる
		.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offset
	};

	// ルートパラメータを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(4);
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	// CBVを使う。b0のbと一致する
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;
	// レジスタ番号0とバインド。b0の0と一致する。もしb11と紐づけたいなら11となる

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges =
		descriptorRange;
	// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(
		descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // バイリニアフィルタ
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP, // 0~1の範囲外をリピート
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER, // 比較しない
			.MaxLOD = D3D12_FLOAT32_MAX,                   // ありったけのMipmapを使う
			.ShaderRegister = 0,                           // レジスタ番号0を使う
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
			// PixelShaderで使う
		}
	};

	// ルートシグネチャを作成
	rootSignatureManager_->CreateRootSignature(
		"SpriteCommon", rootParameters, staticSamplers,
		_countof(staticSamplers)
	);

	if (rootSignatureManager_->Get("SpriteCommon")) {
		Console::Print("SpriteCommon : ルートシグネチャの生成に成功.\n",
		               kConTextColorCompleted, Channel::Engine);
	}
}

void SpriteCommon::CreateGraphicsPipeline() {
	CreateRootSignature();

	// パイプラインステートを作成
	pipelineState_ = PipelineState(
		D3D12_CULL_MODE_NONE,
		D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
	);
	pipelineState_.SetInputLayout(Vertex::inputLayout);
	pipelineState_.SetRootSignature(
		rootSignatureManager_->Get("SpriteCommon"));

	// シェーダーのファイルパスを設定
	pipelineState_.SetVertexShader(
		L"./content/core/shaders/SpriteCommon.VS.hlsl");
	pipelineState_.SetPixelShader(
		L"./content/core/shaders/SpriteCommon.PS.hlsl");
	pipelineState_.SetBlendMode(kBlendModeNormal);
	pipelineState_.Create(d3d12_->GetDevice());

	if (pipelineState_.Get()) {
		Console::Print("SpriteCommon : パイプラインステートの作成に成功.\n",
		               kConTextColorCompleted, Channel::Engine);
	}
}

void SpriteCommon::Render() const {
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	d3d12_->GetCommandList()->SetGraphicsRootSignature(
		rootSignatureManager_->Get("SpriteCommon"));
	d3d12_->GetCommandList()->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
