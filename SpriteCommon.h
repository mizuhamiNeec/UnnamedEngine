#pragma once
#include "RootSignatureManager.h"
#include "Source/Engine/Renderer/D3D12.h"
#include "Source/Engine/Renderer/PipelineState.h"

class SpriteCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;
	void CreateRootSignature();

	void CreateGraphicsPipeline();

	void Render();

	D3D12* GetD3D12() const { return d3d12_; }

private:
	D3D12* d3d12_ = nullptr;
	RootSignatureManager* rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
};
