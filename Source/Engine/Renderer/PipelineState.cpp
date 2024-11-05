#include "PipelineState.h"

#include <cassert>
#include <format>
#include "../Lib/Console/Console.h"
#include "../Lib/Utils/ConvertString.h"

PipelineState::PipelineState() {
}

PipelineState::PipelineState(
	const D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK,
	const D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID
) {
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	rasterizerDesc.CullMode = cullMode; // 裏面(時計回り)を表示しない
	rasterizerDesc.FillMode = fillMode; // 三角形の中を塗りつぶす

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = true; // Depthの機能を有効化する
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込みします
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較関数はLessEqual。つまり、近ければ描画される

	// ステートの設定
	desc_.BlendState = blendDesc; // BlendState
	desc_.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	desc_.NumRenderTargets = 1;
	desc_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 利用するトポロジ(形状)のタイプ。三角形
	desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	desc_.SampleDesc.Count = 1;
	desc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc_.DepthStencilState = depthStencilDesc;
	desc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// DXCの初期化
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	if (FAILED(hr)) {
		Console::Print("Failed to create DxcUtils instance\n", {1.0f, 0.0f, 0.0f, 1.0f});
		return;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	if (FAILED(hr)) {
		Console::Print("Failed to create DxcCompiler instance\n", {1.0f, 0.0f, 0.0f, 1.0f});
		return;
	}

	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	if (FAILED(hr)) {
		Console::Print("Failed to create default include handler\n", {1.0f, 0.0f, 0.0f, 1.0f});
		return;
	}
}

void PipelineState::SetInputLayout(const D3D12_INPUT_LAYOUT_DESC layout) {
	desc_.InputLayout = layout; // InputLayout
}

void PipelineState::SetRootSignature(ID3D12RootSignature* rootSignature) {
	desc_.pRootSignature = rootSignature; // RootSignature
}

void PipelineState::SetVS(const std::wstring& filePath) {
	vsBlob = CompileShader(filePath, L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	assert(vsBlob != nullptr);

	desc_.VS = {
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()
	}; // VertexShader
}

void PipelineState::SetPS(const std::wstring& filePath) {
	psBlob = CompileShader(filePath, L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
	assert(psBlob != nullptr);

	desc_.PS = {
		psBlob->GetBufferPointer(), psBlob->GetBufferSize()
	}; // PixelShader
}

IDxcBlob* PipelineState::CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils,
                                       IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler) {
	/* 1. hlslファイルを読む */
	// これからシェーダーをコンパイルする旨をログに出す
	Console::Print(
		ConvertString::ToString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)),
		kConsoleColorWait);
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
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
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Console::Print(shaderError->GetStringPointer(), kConsoleColorError);
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	/* 4. Compile結果を受け取って返す */
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したらログを出す
	Console::Print(ConvertString::ToString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)),
	               kConsoleColorCompleted);
	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

void PipelineState::Create(ID3D12Device* device) {
	HRESULT hr = device->CreateGraphicsPipelineState(&desc_, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(hr));
	if (SUCCEEDED(hr)) {
		//Console::Print("Complete Create PipelineState.\n", kConsoleColorCompleted);
	}
}

void PipelineState::SetBlendMode(const BlendMode blendMode) {
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	switch (blendMode) {
	case kBlendModeNone:
		rtBlendDesc.BlendEnable = FALSE;
		break;
	case kBlendModeNormal:
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeAdd:
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeSubtract:
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rtBlendDesc.DestBlend = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_SUBTRACT;
		break;
	case kBlendModeMultiply:
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_ZERO;
		rtBlendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeScreen:
		rtBlendDesc.BlendEnable = TRUE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case kCountOfBlendMode:
	default:
		break;
	}
	blendDesc.RenderTarget[0] = rtBlendDesc;
	desc_.BlendState = blendDesc;
	currentBlendMode = blendMode;
}

BlendMode PipelineState::GetBlendMode() {
	return currentBlendMode;
}

ID3D12PipelineState* PipelineState::Get() const {
	return pipelineState.Get();
}

void PipelineState::SetDepthWriteMask(const D3D12_DEPTH_WRITE_MASK depthWriteMask) {
	desc_.DepthStencilState.DepthWriteMask = depthWriteMask;
}
