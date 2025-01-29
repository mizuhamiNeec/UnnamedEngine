#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

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

	ComPtr<IDxcBlob> GetVertexShaderBlob();
	ComPtr<IDxcBlob> GetPixelShaderBlob();
	ComPtr<IDxcBlob> GetGeometryShaderBlob();

	UINT GetResourceRegister(const std::string& resourceName) const;
	const std::unordered_map<std::string, ResourceInfo>& GetResourceRegisterMap() const;
	std::string GetName();

	void Release();
	static void ReleaseStaticResources();

private:
	static ComPtr<IDxcBlob> CompileShader(
		const std::string& filePath,
		const std::string& entryPoint,
		const std::string& profile
	);

	void ReflectShaderBlob(const ComPtr<IDxcBlob>& shaderBlob, ShaderType shaderType);
	void ReflectShaderResources();

	ComPtr<IDxcBlob> vertexShaderBlob_;
	ComPtr<IDxcBlob> pixelShaderBlob_;
	ComPtr<IDxcBlob> geometryShaderBlob_;

	static ComPtr<IDxcUtils> dxcUtils_;
	static ComPtr<IDxcCompiler3> dxcCompiler_;
	static ComPtr<IDxcIncludeHandler> includeHandler_;

	std::string name_;

	std::unordered_map<std::string, ResourceInfo> resourceRegisterMap_; // リソース名とレジスタ番号のマップ
};

