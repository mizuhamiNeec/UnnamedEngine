#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "SrvManager.h"

class ShaderResourceViewManager;

struct PostProcessParams {
	float bloomStrength       = 2.0f;
	float bloomThreshold      = 0.0f;
	float vignetteStrength    = 0.9f;
	float vignetteRadius      = 0.0f;
	float chromaticAberration = 0.02f;
	float padding[3]; // 16バイト
};

class CopyImagePass {
public:
	CopyImagePass(ID3D12Device* device, SrvManager* srvManager);
	~CopyImagePass();

	void Init();

	void Update(const float deltaTime);

	void Execute(
		ID3D12GraphicsCommandList*  commandList,
		ID3D12Resource*             srcTexture,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv
	);

	void Shutdown() { device_ = nullptr; }
	
	// SRVインデックスを取得するメソッドを追加
	uint32_t GetSrvIndex() const { return srvIndex_; }

private:
	void CreateRootSignature();
	void CreatePipelineState();

	SrvManager* srvManager_ = nullptr;
	uint32_t    srvIndex_   = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12Device*                               device_;

	PostProcessParams                      postProcessParams_;
	Microsoft::WRL::ComPtr<ID3D12Resource> postProcessParamsCB_;
};
