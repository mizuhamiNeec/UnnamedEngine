#pragma once
#include <d3d12.h>
#include <string>

#include <wrl/client.h>
#include <unordered_map>

class PipelineManager {
public:
	static size_t CalculatePSOHash(
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

	static ID3D12PipelineState* GetOrCreatePipelineState(
		ID3D12Device*      device,
		const std::string& key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
	);

	static void Shutdown();

private:
	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<
		                          ID3D12PipelineState>> mPipelineStates;
	static std::unordered_map<size_t, Microsoft::WRL::ComPtr<
		                          ID3D12PipelineState>> mPipelineStatesByHash;
};
