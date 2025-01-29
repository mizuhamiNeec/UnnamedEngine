#include "PipelineManager.h"

#include <cassert>

#include <Lib/Console/Console.h>

#include "Lib/Utils/StrUtils.h"

size_t PipelineManager::CalculatePSOHash(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
	std::hash<size_t> hasher;
	size_t hash = 0;

	// 各メンバのハッシュを組み合わせる
	hash = hasher(reinterpret_cast<size_t>(desc.pRootSignature));

	// シェーダーのハッシュ
	if (desc.VS.pShaderBytecode) {
		hash ^= hasher(std::hash<size_t>{}(reinterpret_cast<size_t>(desc.VS.pShaderBytecode))) << 1;
		hash ^= hasher(desc.VS.BytecodeLength) << 1;
	}
	if (desc.PS.pShaderBytecode) {
		hash ^= hasher(std::hash<size_t>{}(reinterpret_cast<size_t>(desc.PS.pShaderBytecode))) << 2;
		hash ^= hasher(desc.PS.BytecodeLength) << 2;
	}

	hash ^= hasher(static_cast<size_t>(desc.BlendState.RenderTarget[0].BlendEnable)) << 3;
	hash ^= hasher(static_cast<size_t>(desc.RasterizerState.FillMode)) << 4;
	hash ^= hasher(static_cast<size_t>(desc.RasterizerState.CullMode)) << 5;
	hash ^= hasher(static_cast<size_t>(desc.DepthStencilState.DepthEnable)) << 6;
	hash ^= hasher(static_cast<size_t>(desc.DepthStencilState.DepthWriteMask)) << 7;
	hash ^= hasher(static_cast<size_t>(desc.DepthStencilState.DepthFunc)) << 8;
	hash ^= hasher(static_cast<size_t>(desc.NumRenderTargets)) << 9;
	hash ^= hasher(static_cast<size_t>(desc.SampleDesc.Count)) << 10;
	hash ^= hasher(static_cast<size_t>(desc.SampleMask)) << 11;
	hash ^= hasher(static_cast<size_t>(desc.PrimitiveTopologyType)) << 12;
	hash ^= hasher(static_cast<size_t>(desc.DSVFormat)) << 13;
	hash ^= hasher(static_cast<size_t>(desc.RTVFormats[0])) << 14;

	return hash;
}

ID3D12PipelineState* PipelineManager::GetOrCreatePipelineState(
	ID3D12Device* device,
	const std::string& key,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
) {
	size_t psoHash = CalculatePSOHash(desc);

	// ハッシュ値で作成済みのPSOを検索
	auto it = pipelineStatesByHash_.find(psoHash);
	if (it != pipelineStatesByHash_.end()) {
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

	// ハッシュとキーの両方を登録
	pipelineStatesByHash_[psoHash] = pipelineState;
	pipelineStates_[key] = pipelineState;

	Console::Print(
		"PSOを作成しました: " + key + "\n",
		kConsoleColorCompleted,
		Channel::RenderPipeline
	);

	pipelineState->SetName(StrUtils::ToWString(key).c_str());

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

	for (auto& pipelineStatesByHash : pipelineStatesByHash_) {
		if (pipelineStatesByHash.second) {
			pipelineStatesByHash.second.Reset();
		}
	}
	pipelineStatesByHash_.clear();
}

std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PipelineManager::pipelineStates_;
std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> PipelineManager::pipelineStatesByHash_;
