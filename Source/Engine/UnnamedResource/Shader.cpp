#include "Shader.h"

#include <cassert>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <Lib/Console/Console.h>
#include <Lib/Utils/StrUtils.h>

//-----------------------------------------------------------------------------
// Purpose: コンストラクタ
// - vsPath (const std::string&): 頂点シェーダーファイルパス
// - psPath (const std::string&): ピクセルシェーダーファイルパス
// - gsPath (const std::string&): ジオメトリシェーダーファイルパス
//-----------------------------------------------------------------------------
Shader::Shader(
	const std::string& vsPath, const std::string& psPath,
	const std::string& gsPath
) {
	HRESULT hr = DxcCreateInstance(
		CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_)
	);
	if (FAILED(hr)) {
		Console::Print(
			"DxcUtilsインスタンスの作成に失敗しました\n",
			kConsoleColorError,
			Channel::RenderSystem
		);
		return;
	}
	hr = DxcCreateInstance(
		CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_)
	);
	if (FAILED(hr)) {
		Console::Print(
			"DxcCompilerインスタンスの作成に失敗しました\n",
			kConsoleColorError,
			Channel::RenderSystem
		);
		return;
	}
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	if (FAILED(hr)) {
		Console::Print(
			"IncludeHandlerの作成に失敗しました\n", kConsoleColorError,
			Channel::RenderSystem
		);
		return;
	}

	if (!vsPath.empty()) {
		vertexShaderBlob_ = CompileShader(
			vsPath,
			"main",
			"vs_6_0"
		);
	}
	if (!psPath.empty()) {
		pixelShaderBlob_ = CompileShader(
			psPath,
			"main",
			"ps_6_0"
		);
	}
	if (!gsPath.empty()) {
		geometryShaderBlob_ = CompileShader(
			gsPath,
			"main",
			"gs_6_0"
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 頂点シェーダーバイナリを取得します
//-----------------------------------------------------------------------------
ComPtr<IDxcBlob> Shader::GetVertexShaderBlob() {
	return vertexShaderBlob_;
}

//-----------------------------------------------------------------------------
// Purpose: ピクセルシェーダーバイナリを取得します
//-----------------------------------------------------------------------------
ComPtr<IDxcBlob> Shader::GetPixelShaderBlob() {
	return pixelShaderBlob_;
}

//-----------------------------------------------------------------------------
// Purpose: ジオメトリシェーダーバイナリを取得します
//-----------------------------------------------------------------------------
ComPtr<IDxcBlob> Shader::GetGeometryShaderBlob() {
	return geometryShaderBlob_;
}

UINT Shader::GetResourceRegister(const std::string& resourceName) const {
	auto it = resourceRegisterMap_.find(resourceName);
	if (it != resourceRegisterMap_.end()) {
		return it->second;
	}

	// リソースが見つからなかった
	Console::Print(
		"リソースが見つかりません: " + resourceName + "\n",
		kConsoleColorError,
		Channel::RenderSystem
	);

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: シェーダーをコンパイルします
// - filePath (const std::string&): ファイルパス
// - entryPoint (const std::string&): エントリーポイント
// - profile (const std::string&): プロファイル
// Return: コンパイルされたバイナリ
//-----------------------------------------------------------------------------
ComPtr<IDxcBlob> Shader::CompileShader(
	const std::string& filePath, const
	std::string& entryPoint,
	const std::string& profile
) {
	std::wstring wFilePath = StrUtils::ToWString(filePath);
	// HLSLファイルを読み込む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils_->LoadFile(
		wFilePath.c_str(), nullptr, &shaderSource
	);
	if (FAILED(hr)) {
		Console::Print(
			"HLSLファイルの読み込みに失敗しました",
			kConsoleColorError,
			Channel::RenderSystem
		);
		return nullptr;
	}
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	const std::wstring wEntryPoint = StrUtils::ToWString(entryPoint);
	const std::wstring wProfile = StrUtils::ToWString(profile);
	// コンパイルする
	LPCWSTR arguments[] = {
		wFilePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", wEntryPoint.c_str(), // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", wProfile.c_str(), // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};

	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler_->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments, // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler_.Get(), // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	if (FAILED(hr)) {
		Console::Print(
			"DXCの起動に失敗しました\n",
			kConsoleColorError,
			Channel::RenderSystem
		);
		assert(SUCCEEDED(hr));
	}

	// 警告・エラーが出ていないか確認する
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(
		DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr
	);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Console::Print(
			shaderError->GetStringPointer(), kConsoleColorError,
			Channel::RenderSystem
		);
		assert(false);
	}

	// コンパイル結果を受け取って返す
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(
		DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr
	);
	assert(SUCCEEDED(hr));
	// 成功したらログを出す
	Console::Print(
		StrUtils::ToString(
			std::format(
				L"コンパイル成功! path: {}, profile: {}\n",
				wFilePath,
				wProfile
			)
		),
		kConsoleColorCompleted,
		Channel::RenderSystem
	);

	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

void Shader::ReflectShaderBlob(const ComPtr<IDxcBlob> shaderBlob) {
	if (!shaderBlob) return;

	ComPtr<ID3D12ShaderReflection> reflector;
	D3DReflect(shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_PPV_ARGS(&reflector)
	);

	D3D12_SHADER_DESC shaderDesc;
	reflector->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		reflector->GetResourceBindingDesc(i, &bindDesc);

		resourceRegisterMap_[bindDesc.Name] = bindDesc.BindPoint;
	}
}

void Shader::ReflectShaderResources() {
	ReflectShaderBlob(vertexShaderBlob_);
	ReflectShaderBlob(pixelShaderBlob_);
	if (geometryShaderBlob_) {
		ReflectShaderBlob(geometryShaderBlob_);
	}
}

ComPtr<IDxcUtils> Shader::dxcUtils_ = nullptr;
ComPtr<IDxcCompiler3> Shader::dxcCompiler_ = nullptr;
ComPtr<IDxcIncludeHandler> Shader::includeHandler_ = nullptr;
