

#include <ranges>

#include <engine/public/ResourceSystem/RootSignature/RootSignature2.h>
#include <engine/public/ResourceSystem/RootSignature/RootSignatureManager2.h>

RootSignature2* RootSignatureManager2::GetOrCreateRootSignature(const std::string& key, const RootSignatureDesc& desc) {
	// 既に作成済みのルートシグネチャがあるか確認
	if (rootSignatures_.contains(key)) {
		return rootSignatures_[key].get();
	}

	// なかったので作成
	auto rootSignature = std::make_unique<RootSignature2>();
	rootSignature->Init(device_, desc);

	// 作成したルートシグネチャを登録
	rootSignatures_[key] = std::move(rootSignature);

	return rootSignatures_[key].get();
}

void RootSignatureManager2::Init(ID3D12Device* device) {
	device_ = device;
}

void RootSignatureManager2::Shutdown() {
	// 各RootSignature2インスタンスが保持しているリソースを解放
	for (auto& rootSignature : rootSignatures_ | std::views::values) {
		if (rootSignature) {
			// RootSignature内のComPtrを明示的にリセット
			rootSignature->Release();
			rootSignature.reset();
		}
	}

	// マップをクリア
	rootSignatures_.clear();

	// デバイスへの参照をクリア
	device_ = nullptr;
}

std::unordered_map<std::string, std::unique_ptr<RootSignature2>> RootSignatureManager2::rootSignatures_;
ID3D12Device* RootSignatureManager2::device_ = nullptr;