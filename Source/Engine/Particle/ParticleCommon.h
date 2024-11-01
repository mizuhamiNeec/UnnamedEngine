#pragma once

#include "../Renderer/PipelineState.h"

class RootSignatureManager;
class D3D12;

class ParticleCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;
	void CreateRootSignature();

	void CreateGraphicsPipeline();

	void Render() const;

	D3D12* GetD3D12() const { return d3d12_; }

private:
	D3D12* d3d12_ = nullptr;
	RootSignatureManager* rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
};
