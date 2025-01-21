#include "PipelineManager.h"

#include <cassert>

#include <Lib/Console/Console.h>

ID3D12PipelineState* PipelineManager::GetOrCreatePipelineState(
	ID3D12Device* device,
	const std::string& key,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
) {
	 // 既存のパイプラインステートをチェック
	auto it = pipelineStates_.find(key);
	if (it != pipelineStates_.end()) {
		return it->second.Get();
	}

	// ルートシグネチャの確認
	if (!desc.pRootSignature) {
		Console::Print(
			"ルートシグネチャが設定されていません: " + key + "\n",
			kConsoleColorError,
			Channel::RenderPipeline
		);
		return nullptr;
	}

	// シェーダーの確認
	if (!desc.VS.pShaderBytecode || !desc.VS.BytecodeLength) {
		Console::Print(
			"頂点シェーダーが設定されていません: " + key + "\n",
			kConsoleColorError,
			Channel::RenderPipeline
		);
		return nullptr;
	}

	// パイプラインステートの作成
	ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(hr)) {
		Console::Print(
			std::format("PSOの作成に失敗しました: {} (HRESULT: {:08X})\n", key, static_cast<unsigned int>(hr)),
			kConsoleColorError,
			Channel::RenderPipeline
		);
		return nullptr;
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

void PipelineManager::Shutdown() {
	Console::Print("Pipeline Manager を終了しています...\n", kConsoleColorWait, Channel::ResourceSystem);

   // 各パイプラインステートを解放
	for (auto& [key, pipelineState] : pipelineStates_) {
		if (pipelineState) {
			pipelineState.Reset();
		}
	}
	pipelineStates_.clear();
}

std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PipelineManager::pipelineStates_;
