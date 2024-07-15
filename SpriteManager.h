#pragma once
#include "Source/Engine/Renderer/D3D12.h"
#include "Source/Engine/Renderer/PipelineState.h"
#include "Source/Engine/Renderer/RootSignature.h"

class SpriteManager {
public:
	void Init(D3D12* renderer);

private:
	void CreateRootSignature();
	void CreateGraphicsPipeline();

	D3D12* renderer_ = nullptr;
	PipelineState* pipelineState_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
