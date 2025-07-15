#include "Shader.h"

#include <cassert>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <SubSystem/Console/Console.h>
#include <Lib/Utils/StrUtil.h>

#ifndef DFCC_DXIL
#define DFCC_DXIL MAKEFOURCC('D', 'X', 'I', 'L')
#endif
#include <format>


//-----------------------------------------------------------------------------
// Purpose: コンストラクタ
// - vsPath (const std::string&): 頂点シェーダーファイルパス
// - psPath (const std::string&): ピクセルシェーダーファイルパス
// - gsPath (const std::string&): ジオメトリシェーダーファイルパス
//-----------------------------------------------------------------------------
Shader::Shader(
	std::string        name, const std::string&   vsPath,
	const std::string& psPath, const std::string& gsPath
) : mName(std::move(name)) {
	HRESULT hr = DxcCreateInstance(
		CLSID_DxcUtils, IID_PPV_ARGS(&mDxcUtils)
	);
	if (FAILED(hr)) {
		Console::Print(
			"DxcUtilsインスタンスの作成に失敗しました\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}
	hr = DxcCreateInstance(
		CLSID_DxcCompiler, IID_PPV_ARGS(&mDxcCompiler)
	);
	if (FAILED(hr)) {
		Console::Print(
			"DxcCompilerインスタンスの作成に失敗しました\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}
	hr = mDxcUtils->CreateDefaultIncludeHandler(&mIncludeHandler);
	if (FAILED(hr)) {
		Console::Print(
			"IncludeHandlerの作成に失敗しました\n", kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	if (!vsPath.empty()) {
		mVertexShaderBlob = CompileShader(
			vsPath,
			"VSMain",
			"vs_6_0"
		);
	}
	if (!psPath.empty()) {
		mPixelShaderBlob = CompileShader(
			psPath,
			"PSMain",
			"ps_6_0"
		);
	}
	if (!gsPath.empty()) {
		mGeometryShaderBlob = CompileShader(
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
Microsoft::WRL::ComPtr<IDxcBlob> Shader::GetVertexShaderBlob() {
	return mVertexShaderBlob;
}

//-----------------------------------------------------------------------------
// Purpose: ピクセルシェーダーバイナリを取得します
//-----------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IDxcBlob> Shader::GetPixelShaderBlob() {
	return mPixelShaderBlob;
}

//-----------------------------------------------------------------------------
// Purpose: ジオメトリシェーダーバイナリを取得します
//-----------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IDxcBlob> Shader::GetGeometryShaderBlob() {
	return mGeometryShaderBlob;
}

UINT Shader::GetResourceRegister(const std::string& resourceName) const {
	auto it = mResourceRegisterMap.find(resourceName);
	if (it != mResourceRegisterMap.end()) {
		return it->second.bindPoint;
	}

	// リソースが見つからなかった
	Console::Print(
		"リソースが見つかりません: " + resourceName + "\n",
		kConTextColorError,
		Channel::RenderSystem
	);

	return 0xffffffff;
}

const std::unordered_map<std::string, ResourceInfo>&
Shader::GetResourceRegisterMap() const {
	return mResourceRegisterMap;
}

std::string Shader::GetName() {
	return mName;
}

UINT Shader::GetResourceParameterIndex(const std::string& resourceName) {
	auto it = mResourceParameterIndices.find(resourceName);
	if (it != mResourceParameterIndices.end()) {
		return it->second;
	}

	Console::Print(
		"リソースが見つかりません: " + resourceName + "\n",
		kConTextColorError,
		Channel::RenderSystem
	);
	return UINT_MAX;
}

void Shader::SetResourceParameterIndex(const std::string& resourceName,
                                       const UINT         index) {
	mResourceParameterIndices[resourceName] = index;
}

void Shader::Release() {
	if (mVertexShaderBlob) {
		mVertexShaderBlob.Reset();
	}
	if (mPixelShaderBlob) {
		mPixelShaderBlob.Reset();
	}
	if (mGeometryShaderBlob) {
		mGeometryShaderBlob.Reset();
	}

	mResourceRegisterMap.clear();
}

void Shader::ReleaseStaticResources() {
	if (mIncludeHandler) {
		mIncludeHandler.Reset();
	}
	if (mDxcCompiler) {
		mDxcCompiler.Reset();
	}
	if (mDxcUtils) {
		mDxcUtils.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: シェーダーをコンパイルします
// - filePath (const std::string&): ファイルパス
// - entryPoint (const std::string&): エントリーポイント
// - profile (const std::string&): プロファイル
// Return: コンパイルされたバイナリ
//-----------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IDxcBlob> Shader::CompileShader(
	const std::string& filePath, const
	std::string&       entryPoint,
	const std::string& profile
) {
	std::wstring wFilePath = StrUtil::ToWString(filePath);
	// HLSLファイルを読み込む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT           hr           = mDxcUtils->LoadFile(
		wFilePath.c_str(), nullptr, &shaderSource
	);
	if (FAILED(hr)) {
		Console::Print(
			"HLSLファイルの読み込みに失敗しました",
			kConTextColorError,
			Channel::RenderSystem
		);
		return nullptr;
	}
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr      = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size     = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	const std::wstring wEntryPoint = StrUtil::ToWString(entryPoint);
	const std::wstring wProfile    = StrUtil::ToWString(profile);
	// コンパイルする
	LPCWSTR arguments[] = {
		wFilePath.c_str(),          // コンパイル対象のhlslファイル名
		L"-E", wEntryPoint.c_str(), // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", wProfile.c_str(),    // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug",   // デバッグ用の情報を埋め込む
		L"-Od",                     // 最適化を外しておく
		L"-Zpr",                    // メモリレイアウトは行優先
	};

	IDxcResult* shaderResult = nullptr;
	hr                       = mDxcCompiler->Compile(
		&shaderSourceBuffer,        // 読み込んだファイル
		arguments,                  // コンパイルオプション
		_countof(arguments),        // コンパイルオプションの数
		mIncludeHandler.Get(),      // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	if (FAILED(hr)) {
		Console::Print(
			"DXCの起動に失敗しました\n",
			kConTextColorError,
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
			shaderError->GetStringPointer(), kConTextColorError,
			Channel::RenderSystem
		);
		assert(false);
	}

	// コンパイル結果を受け取って返す
	IDxcBlob* shaderBlob = nullptr;
	hr                   = shaderResult->GetOutput(
		DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr
	);
	assert(SUCCEEDED(hr));
	// 成功したらログを出す
	Console::Print(
		StrUtil::ToString(
			std::format(
				L"コンパイル成功! path: {}, profile: {}\n",
				wFilePath,
				wProfile
			)
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

void Shader::ReflectShaderBlob(
	const Microsoft::WRL::ComPtr<IDxcBlob>& shaderBlob,
	ShaderType                              shaderType) {
	if (!shaderBlob) {
		Console::Print("shaderBlobがnullです\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	Microsoft::WRL::ComPtr<IDxcContainerReflection> containerReflection;
	HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection,
	                               IID_PPV_ARGS(&containerReflection));
	if (FAILED(hr)) {
		Console::Print("DXCリフレクションの作成に失敗しました\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	hr = containerReflection->Load(shaderBlob.Get());
	if (FAILED(hr)) {
		Console::Print("シェーダーバイナリの読み込みに失敗しました\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	UINT32 dxilPartIndex;
	hr = containerReflection->FindFirstPartKind(DFCC_DXIL, &dxilPartIndex);
	if (FAILED(hr)) {
		Console::Print("DXILパートの検索に失敗しました\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflector;
	hr = containerReflection->GetPartReflection(
		dxilPartIndex, IID_PPV_ARGS(&reflector));
	if (FAILED(hr)) {
		Console::Print("シェーダーリフレクションの作成に失敗しました\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	D3D12_SHADER_DESC shaderDesc;
	hr = reflector->GetDesc(&shaderDesc);
	if (FAILED(hr)) {
		Console::Print("シェーダー記述子の取得に失敗しました\n", kConTextColorError,
		               Channel::RenderSystem);
		return;
	}

	std::unordered_map<std::string, ResourceInfo> stageResources;

	// バウンドリソースの解析
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		hr = reflector->GetResourceBindingDesc(i, &bindDesc);
		if (SUCCEEDED(hr)) {
			std::string  name = bindDesc.Name;
			ResourceInfo info;
			info.bindPoint = bindDesc.BindPoint;
			info.type      = bindDesc.Type;

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
				kConTextColorCompleted,
				Channel::RenderSystem
			);
		}
	}

	// グローバルリソースマップとのマージ
	for (const auto& [name, info] : stageResources) {
		auto it = mResourceRegisterMap.find(name);
		if (it != mResourceRegisterMap.end()) {
			// 既存のリソースがある場合
			if (it->second.type == info.type && it->second.bindPoint == info.
				bindPoint) {
				// 同じリソースが別のステージで使用される場合
				if (it->second.visibility != info.visibility) {
					it->second.visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		} else {
			// 新しいリソースとして追加
			mResourceRegisterMap[name] = info;
		}
	}
}

void Shader::ReflectShaderResources() {
	if (mVertexShaderBlob) {
		ReflectShaderBlob(mVertexShaderBlob, ShaderType::VertexShader);
	}
	if (mPixelShaderBlob) {
		ReflectShaderBlob(mPixelShaderBlob, ShaderType::PixelShader);
	}
	if (mGeometryShaderBlob) {
		ReflectShaderBlob(mGeometryShaderBlob, ShaderType::GeometryShader);
	}
}

std::vector<std::string> Shader::GetTextureSlots() const {
	std::vector<std::string> textureSlots;

	// リソースマップからテクスチャに関連するリソース名を抽出
	for (const auto& [resourceName, info] : mResourceRegisterMap) {
		// テクスチャっぽいリソース名を抽出（baseColorTextureなど）
		if (resourceName.find("Texture") != std::string::npos ||
			resourceName.find("Map") != std::string::npos ||
			info.type == D3D_SIT_TEXTURE) {
			textureSlots.emplace_back(resourceName);
		}
	}

	// デフォルトのテクスチャスロット名を追加（存在しなかった場合）
	if (textureSlots.empty()) {
		// もし特別にリストアップされていない場合は、一般的なテクスチャスロット名を返す
		textureSlots.emplace_back("gMainTexture");
		textureSlots.emplace_back("gBaseColorTexture");
	}

	return textureSlots;
}

Microsoft::WRL::ComPtr<IDxcUtils>          Shader::mDxcUtils       = nullptr;
Microsoft::WRL::ComPtr<IDxcCompiler3>      Shader::mDxcCompiler    = nullptr;
Microsoft::WRL::ComPtr<IDxcIncludeHandler> Shader::mIncludeHandler = nullptr;
