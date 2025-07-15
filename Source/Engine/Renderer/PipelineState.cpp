#include "PipelineState.h"

#include <cassert>
#include <d3d12.h>
#include <dxgiformat.h>
#include <winnt.h>
#include <dxcapi.h>
#include <format>
#include <string>
#include "../SubSystem/Console/Console.h"
#include "../Lib/Utils/StrUtil.h"

#include "Lib/Utils/ClientProperties.h"

PipelineState::PipelineState() = default;

PipelineState::PipelineState(
	const D3D12_CULL_MODE cullMode,
	const D3D12_FILL_MODE fillMode,
	const D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
	const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc
) {
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	mRasterizerDesc.CullMode = cullMode; // 裏面(時計回り)を表示しない
	mRasterizerDesc.FillMode = fillMode; // 三角形の中を塗りつぶす

	// ステートの設定
	mDesc.BlendState = blendDesc; // BlendState
	mDesc.RasterizerState = mRasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	mDesc.NumRenderTargets = 1;
	mDesc.RTVFormats[0] = kBufferFormat;
	// 利用するトポロジ(形状)のタイプ。三角形
	mDesc.PrimitiveTopologyType = topologyType;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	mDesc.SampleDesc.Count = 1;
	mDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	mDesc.DepthStencilState = depthStencilDesc;
	mDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// DXCの初期化
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mDxcUtils));
	if (FAILED(hr)) {
		Console::Print(
			"Failed to create DxcUtils instance\n", {
				1.0F, 0.0F, 0.0F, 1.0F
			}
		);
		return;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mDxcCompiler));
	if (FAILED(hr)) {
		Console::Print(
			"Failed to create DxcCompiler instance\n", {
				1.0F, 0.0F, 0.0F, 1.0F
			}
		);
		return;
	}

	hr = mDxcUtils->CreateDefaultIncludeHandler(&mIncludeHandler);
	if (FAILED(hr)) {
		Console::Print(
			"Failed to create default include handler\n", {
				1.0F, 0.0F, 0.0F, 1.0F
			}
		);
		return;
	}
}

void PipelineState::SetInputLayout(const D3D12_INPUT_LAYOUT_DESC layout) {
	mDesc.InputLayout = layout; // InputLayout
}

void PipelineState::SetRootSignature(ID3D12RootSignature* rootSignature) {
	mDesc.pRootSignature = rootSignature; // RootSignature
}

void PipelineState::SetVertexShader(const std::wstring& filePath) {
	mVsBlob = CompileShader(
		filePath, L"vs_6_0", mDxcUtils.Get(), mDxcCompiler.Get(),
		mIncludeHandler.Get()
	);
	assert(mVsBlob != nullptr);

	mDesc.VS = {
		.pShaderBytecode = mVsBlob->GetBufferPointer(),
		.BytecodeLength = mVsBlob->
		GetBufferSize()
	}; // VertexShader
}

void PipelineState::SetPixelShader(const std::wstring& filePath) {
	mPsBlob = CompileShader(
		filePath, L"ps_6_0", mDxcUtils.Get(), mDxcCompiler.Get(),
		mIncludeHandler.Get()
	);
	assert(mPsBlob != nullptr);

	mDesc.PS = {
		.pShaderBytecode = mPsBlob->GetBufferPointer(),
		.BytecodeLength = mPsBlob->
		GetBufferSize()
	}; // PixelShader
}

IDxcBlob* PipelineState::CompileShader(
	const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler
) {
	/* 1. hlslファイルを読む */
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer {};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	/* 2. Compileする */
	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};

	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments, // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	/* 3. 警告・エラーが出ていないか確認する */
	// 警告・エラーが出てきたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(
		DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr
	);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Console::Print(
			shaderError->GetStringPointer(), kConTextColorError, Channel::Engine
		);
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	/* 4. Compile結果を受け取って返す */
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(
		DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr
	);
	assert(SUCCEEDED(hr));
	// 成功したらログを出す
	Console::Print(
		StrUtil::ToString(
			std::format(
				L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile
			)
		),
		kConTextColorCompleted, Channel::Engine
	);
	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

void PipelineState::Create(ID3D12Device* device) {
	const HRESULT hr = device->CreateGraphicsPipelineState(
		&mDesc, IID_PPV_ARGS(mPipelineState.ReleaseAndGetAddressOf())
	);
	assert(SUCCEEDED(hr));
	if (SUCCEEDED(hr)) {
		Console::Print(
			"Complete Create PipelineState.\n", kConTextColorCompleted,
			Channel::Engine
		);
	}
}

void PipelineState::SetBlendMode(const BlendMode blendMode) {
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	switch (blendMode) {
	case kBlendModeNone: // 不透明
		rtBlendDesc.BlendEnable = FALSE;
	// ブレンドしないので値の設定不要
		break;
	case kBlendModeNormal: // アルファブレンド
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeAdd: // 加算
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
    		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeSubtract: // 減算
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
				rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
        		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeMultiply: // 乗算
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_DEST_COLOR;
		rtBlendDesc.DestBlend = D3D12_BLEND_ZERO;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
    		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeScreen: // スクリーン（発光）
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
    		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kCountOfBlendMode:
	default: break;
	}
	blendDesc.RenderTarget[0] = rtBlendDesc;
	mDesc.BlendState = blendDesc;
	mCurrentBlendMode = blendMode;
}

auto PipelineState::GetBlendMode() const -> BlendMode {
	return mCurrentBlendMode;
}

auto PipelineState::Get() const -> ID3D12PipelineState* {
	return mPipelineState.Get();
}

void PipelineState::SetDepthWriteMask(
	const D3D12_DEPTH_WRITE_MASK depthWriteMask
) {
	mDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
}
