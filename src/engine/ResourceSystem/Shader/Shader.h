#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

struct ResourceInfo {
	UINT bindPoint;
	D3D12_SHADER_VISIBILITY visibility;
	D3D_SHADER_INPUT_TYPE type;
};

enum class ShaderType {
	VertexShader,
	PixelShader,
	GeometryShader
};

class Shader {
public:
	Shader(
		std::string name,
		const std::string& vsPath,
		const std::string& psPath,
		const std::string& gsPath = ""
	);

	Microsoft::WRL::ComPtr<IDxcBlob> GetVertexShaderBlob();
	Microsoft::WRL::ComPtr<IDxcBlob> GetPixelShaderBlob();
	Microsoft::WRL::ComPtr<IDxcBlob> GetGeometryShaderBlob();

	[[nodiscard]] UINT GetResourceRegister(const std::string& resourceName) const;
	[[nodiscard]] const std::unordered_map<std::string, ResourceInfo>& GetResourceRegisterMap() const;
	std::string GetName();
	UINT GetResourceParameterIndex(const std::string& resourceName);
	void SetResourceParameterIndex(const std::string& resourceName, UINT index);
	
	// テクスチャスロット名のリストを取得
	[[nodiscard]] std::vector<std::string> GetTextureSlots() const;

	void Release();
	static void ReleaseStaticResources();

private:
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::string& filePath,
		const std::string& entryPoint,
		const std::string& profile
	);

	void ReflectShaderBlob(const Microsoft::WRL::ComPtr<IDxcBlob>& shaderBlob, ShaderType shaderType);
	void ReflectShaderResources();

	Microsoft::WRL::ComPtr<IDxcBlob> mVertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> mPixelShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> mGeometryShaderBlob;

	static Microsoft::WRL::ComPtr<IDxcUtils> mDxcUtils;
	static Microsoft::WRL::ComPtr<IDxcCompiler3> mDxcCompiler;
	static Microsoft::WRL::ComPtr<IDxcIncludeHandler> mIncludeHandler;

	std::string mName;

	// リソース名とレジスタ番号のマップ
	std::unordered_map<std::string, ResourceInfo> mResourceRegisterMap;

	// リソース名とパラメータインデックスのマップ
	std::unordered_map<std::string, UINT> mResourceParameterIndices;
};

