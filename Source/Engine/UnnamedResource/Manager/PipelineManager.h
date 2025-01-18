#pragma once
#include <d3d12.h>
#include <unordered_map>

#include <UnnamedResource/Shader.h>

class PipelineManager {
public:
	static ID3D12PipelineState* GetOrCreatePipelineState(
		ID3D12Device* device,
		const std::string& key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
	);

private:
	static std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelineStates_;
};
