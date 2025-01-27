#include "Shader.h"

#include <cassert>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <SubSystem/Console/Console.h>
#include <Lib/Utils/StrUtils.h>

#ifndef DFCC_DXIL
#define DFCC_DXIL MAKEFOURCC('D', 'X', 'I', 'L')
#endif	


//-----------------------------------------------------------------------------
// Purpose: コンストラクタ
// - vsPath (const std::string&): 頂点シェーダーファイルパス
// - psPath (const std::string&): ピクセルシェーダーファイルパス
// - gsPath (const std::string&): ジオメトリシェーダーファイルパス
//-----------------------------------------------------------------------------
Shader::Shader(
	std::string name, const std::string& vsPath,
	const std::string& psPath, const std::string& gsPath
) : name_(std::move(name)) {
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
			"VSMain",
			"vs_6_0"
		);
	}
	if (!psPath.empty()) {
		pixelShaderBlob_ = CompileShader(
			psPath,
			"PSMain",
			"ps_6_0"
		);
	}
	if (!gsPath.empty()) {
		geometryShaderBlob_ = CompileShader(
			gsPath,
			"GSMain",
			"gs_6_0"
		);
	}

	// コンパイルが完了したらリソースをリフレクトする
	ReflectShaderResources();
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
		return it->second.bindPoint;
	}

	// リソースが見つからなかった
	Console::Print(
		"リソースが見つかりません: " + resourceName + "\n",
		kConsoleColorError,
		Channel::RenderSystem
	);

	return 0;
}

const std::unordered_map<std::string, ResourceInfo>& Shader::GetResourceRegisterMap() const {
	return resourceRegisterMap_;
}

std::string Shader::GetName() {
	return name_;
}

void Shader::Release() {
	if (vertexShaderBlob_) {
		vertexShaderBlob_.Reset();
	}
	if (pixelShaderBlob_) {
		pixelShaderBlob_.Reset();
	}
	if (geometryShaderBlob_) {
		geometryShaderBlob_.Reset();
	}

	resourceRegisterMap_.clear();
}

void Shader::ReleaseStaticResources() {
	if (includeHandler_) {
		includeHandler_.Reset();
	}
	if (dxcCompiler_) {
		dxcCompiler_.Reset();
	}
	if (dxcUtils_) {
		dxcUtils_.Reset();
	}
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

void Shader::ReflectShaderBlob(const ComPtr<IDxcBlob>& shaderBlob, ShaderType shaderType) {
	if (!shaderBlob) {
		Console::Print("shaderBlobがnullです\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	ComPtr<IDxcContainerReflection> containerReflection;
	HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection));
	if (FAILED(hr)) {
		Console::Print("DXCリフレクションの作成に失敗しました\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	hr = containerReflection->Load(shaderBlob.Get());
	if (FAILED(hr)) {
		Console::Print("シェーダーバイナリの読み込みに失敗しました\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	UINT32 dxilPartIndex;
	hr = containerReflection->FindFirstPartKind(DFCC_DXIL, &dxilPartIndex);
	if (FAILED(hr)) {
		Console::Print("DXILパートの検索に失敗しました\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	ComPtr<ID3D12ShaderReflection> reflector;
	hr = containerReflection->GetPartReflection(dxilPartIndex, IID_PPV_ARGS(&reflector));
	if (FAILED(hr)) {
		Console::Print("シェーダーリフレクションの作成に失敗しました\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	D3D12_SHADER_DESC shaderDesc;
	hr = reflector->GetDesc(&shaderDesc);
	if (FAILED(hr)) {
		Console::Print("シェーダー記述子の取得に失敗しました\n", kConsoleColorError, Channel::RenderSystem);
		return;
	}

	std::unordered_map<std::string, ResourceInfo> stageResources;

	// バウンドリソースの解析
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		hr = reflector->GetResourceBindingDesc(i, &bindDesc);
		if (SUCCEEDED(hr)) {
			std::string name = bindDesc.Name;
			ResourceInfo info;
			info.bindPoint = bindDesc.BindPoint;
			info.type = bindDesc.Type;

			// シェーダーステージに応じたvisibilityを設定
			switch (shaderType) {
			case ShaderType::VertexShader:
				info.visibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case ShaderType::PixelShader:
				info.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case ShaderType::GeometryShader:
				info.visibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
				break;
			default:
				info.visibility = D3D12_SHADER_VISIBILITY_ALL;
				break;
			}

			// ステージごとのリソースマップに追加
			stageResources[name] = info;

			Console::Print(
				std::format(
					"リソースを検出: {} (register {}, type {}, visibility {})\n",
					name,
					info.bindPoint,
					static_cast<int>(info.type),
					static_cast<int>(info.visibility)
				),
				kConsoleColorCompleted,
				Channel::RenderSystem
			);
		}
	}

	// グローバルリソースマップとのマージ
	for (const auto& [name, info] : stageResources) {
		auto it = resourceRegisterMap_.find(name);
		if (it != resourceRegisterMap_.end()) {
			// 既存のリソースがある場合
			if (it->second.type == info.type && it->second.bindPoint == info.bindPoint) {
				// 同じリソースが別のステージで使用される場合
				if (it->second.visibility != info.visibility) {
					it->second.visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		} else {
			// 新しいリソースとして追加
			resourceRegisterMap_[name] = info;
		}
	}

	//// 定数バッファの解析
	//for (UINT i = 0; i < shaderDesc.ConstantBuffers; i++) {
	//	ID3D12ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
	//	D3D12_SHADER_BUFFER_DESC bufferDesc;
	//	hr = cb->GetDesc(&bufferDesc);
	//	if (SUCCEEDED(hr)) {
	//		std::string name = "CB_" + std::string(bufferDesc.Name);
	//		resourceRegisterMap_[name] = { i, D3D12_SHADER_VISIBILITY_ALL };
	//		Console::Print(
	//			std::format("定数バッファを検出: {} (register b{})\n", name, i),
	//			kConsoleColorCompleted,
	//			Channel::RenderSystem
	//		);
	//	}
	//}

	//// バインドリソースの解析
	//for (UINT i = 0; i < shaderDesc.BoundResources; i++) {
	//	D3D12_SHADER_INPUT_BIND_DESC bindDesc;
	//	hr = reflector->GetResourceBindingDesc(i, &bindDesc);
	//	if (SUCCEEDED(hr)) {
	//		std::string name = bindDesc.Name;
	//		if (bindDesc.Type == D3D_SIT_TEXTURE) {
	//			name = "TEX_" + name;
	//		}
	//		resourceRegisterMap_[name] = bindDesc.BindPoint;
	//		Console::Print(
	//			std::format("リソースを検出: {} (register {})\n", name, bindDesc.BindPoint),
	//			kConsoleColorCompleted,
	//			Channel::RenderSystem
	//		);
	//	}
	//}
}

void Shader::ReflectShaderResources() {
	if (vertexShaderBlob_) {
		ReflectShaderBlob(vertexShaderBlob_, ShaderType::VertexShader);
	}
	if (pixelShaderBlob_) {
		ReflectShaderBlob(pixelShaderBlob_, ShaderType::PixelShader);
	}
	if (geometryShaderBlob_) {
		ReflectShaderBlob(geometryShaderBlob_, ShaderType::GeometryShader);
	}
}

ComPtr<IDxcUtils> Shader::dxcUtils_ = nullptr;
ComPtr<IDxcCompiler3> Shader::dxcCompiler_ = nullptr;
ComPtr<IDxcIncludeHandler> Shader::includeHandler_ = nullptr;
