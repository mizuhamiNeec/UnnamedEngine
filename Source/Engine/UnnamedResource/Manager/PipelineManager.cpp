#include "PipelineManager.h"

#include <cassert>

#include <Lib/Console/Console.h>

ID3D12PipelineState* PipelineManager::GetOrCreatePipelineState(
	ID3D12Device* device,
	const std::string& key,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
) {
	// 既に作成済みのパイプラインステートがあるか確認
	auto it = pipelineStates_.find(key);
	if (it != pipelineStates_.end()) {
		return it->second.Get();
	}

	// なかったので作成
	ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(hr)) {
		Console::Print(
			"PSOの作成に失敗しました: " + key + "\n",
			kConsoleColorError,
			Channel::RenderPipeline
		);
		assert(SUCCEEDED(hr));
	}

	// 作成したパイプラインステートを登録
	pipelineStates_[key] = pipelineState;

	Console::Print(
		"PSOを作成しました: " + key + "\n",
		kConsoleColorCompleted,
		Channel::RenderPipeline
	);

	return pipelineState.Get();
}

std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PipelineManager::pipelineStates_;