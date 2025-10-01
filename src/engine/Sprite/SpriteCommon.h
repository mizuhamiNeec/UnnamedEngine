#pragma once

#include <memory>
#include "engine/renderer/RootSignatureManager.h"
#include "engine/renderer/PipelineState.h"

class D3D12;

class SpriteCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;
	void CreateRootSignature();

	void CreateGraphicsPipeline();

	void Render() const;

	D3D12* GetD3D12() const {
		return d3d12_;
	}

private:
	D3D12* d3d12_ = nullptr;
	std::unique_ptr<RootSignatureManager> rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
};
