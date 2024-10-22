#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <string>

using namespace Microsoft::WRL;

class PipelineState {
public:
	PipelineState();
	PipelineState(D3D12_CULL_MODE cullMode, D3D12_FILL_MODE fillMode);
	void SetInputLayout(D3D12_INPUT_LAYOUT_DESC layout);
	void SetRootSignature(ID3D12RootSignature* rootSignature);
	void SetVS(const std::wstring& filePath);
	void SetPS(const std::wstring& filePath);
	static IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils,
	                               IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler);
	void Create(ID3D12Device* device);

	ID3D12PipelineState* Get() const;

private:
	D3D12_RASTERIZER_DESC rasterizerDesc = {};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_ = {};
	ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	ComPtr<IDxcBlob> vsBlob = nullptr;
	ComPtr<IDxcBlob> psBlob = nullptr;

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;
};
