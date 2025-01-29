#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>
#include <wrl/client.h>

using namespace Microsoft::WRL;

enum BlendMode {
	//!< ブレンドなし
	kBlendModeNone = 0,
	kBlendModeNormal, //!< 通常αブレンド Src * SrcA + Dest * (1 - SrcA)
	kBlendModeAdd, //!< 加算 Src * SrcA + Desc * 1
	kBlendModeSubtract, //!< 減算 Desc * 1 - Src * SrcA
	kBlendModeMultiply, //!< 乗算 Src * 0 + Dest * Src
	kBlendModeScreen, //!< スクリーン。 Src * (1 - Dest) + Desc * 1
	kCountOfBlendMode, // ブレンドモード数
};

class PipelineState {
public:
	PipelineState();
	PipelineState(D3D12_CULL_MODE cullMode, D3D12_FILL_MODE fillMode, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType);
	void SetInputLayout(D3D12_INPUT_LAYOUT_DESC layout);
	void SetRootSignature(ID3D12RootSignature* rootSignature);
	void SetVS(const std::wstring& filePath);
	void SetPS(const std::wstring& filePath);
	static IDxcBlob* CompileShader(
		const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler
	);
	void Create(ID3D12Device* device);
	void SetBlendMode(BlendMode blendMode);
	BlendMode GetBlendMode() const;

	ID3D12PipelineState* Get() const;
	void SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK depthWriteMask);

private:
	D3D12_RASTERIZER_DESC rasterizerDesc = {};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_ = {};
	ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	ComPtr<IDxcBlob> vsBlob = nullptr;
	ComPtr<IDxcBlob> psBlob = nullptr;

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;

	BlendMode currentBlendMode = kBlendModeNone;
};
