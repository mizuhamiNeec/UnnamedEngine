#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class Shader {
public:
	Shader(
		const std::string& vsPath,
		const std::string& psPath,
		const std::string& gsPath = ""
	);

	ComPtr<IDxcBlob> GetVertexShaderBlob();
	ComPtr<IDxcBlob> GetPixelShaderBlob();
	ComPtr<IDxcBlob> GetGeometryShaderBlob();

	UINT GetResourceRegister(const std::string& resourceName) const;

private:
	static ComPtr<IDxcBlob> CompileShader(
		const std::string& filePath,
		const std::string& entryPoint,
		const std::string& profile
	);

	void ReflectShaderBlob(ComPtr<IDxcBlob> shaderBlob);
	void ReflectShaderResources();

	ComPtr<IDxcBlob> vertexShaderBlob_;
	ComPtr<IDxcBlob> pixelShaderBlob_;
	ComPtr<IDxcBlob> geometryShaderBlob_;

	static ComPtr<IDxcUtils> dxcUtils_;
	static ComPtr<IDxcCompiler3> dxcCompiler_;
	static ComPtr<IDxcIncludeHandler> includeHandler_;

	std::unordered_map<std::string, UINT> resourceRegisterMap_; // リソース名とレジスタ番号のマップ
};

