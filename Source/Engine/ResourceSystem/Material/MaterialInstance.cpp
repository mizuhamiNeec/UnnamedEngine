#include "MaterialInstance.h"

#include "ResourceSystem/Texture/TextureManager.h"

void MaterialInstance::Apply(
	ID3D12GraphicsCommandList* commandList,
	UINT rootParameterIndex,
	ShaderResourceViewManager* srvManager,
	const std::vector<std::string>& textureOrder) {
	// 各スロットのGPUディスクリプタハンドルを格納する配列を用意
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles;

	for (const auto& slot : textureOrder) {
		Texture* texture = GetTexture(slot);
		// 未設定の場合はエラーテクスチャを設定する
		if (!texture) {
			texture = TextureManager::GetErrorTexture().get();
		}
		gpuHandles.push_back(texture->GetShaderResourceView());
	}

	// SRVマネージャで連続領域へディスクリプタをコピー
	// その開始ハンドルを使ってルートパラメータを設定
	srvManager->BindDescriptorTable(gpuHandles, commandList, rootParameterIndex);
}
