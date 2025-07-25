

#include "engine/public/ResourceSystem/Material/MaterialInstance.h"

#include "engine/public/OldConsole/Console.h"
#include "engine/public/renderer/SrvManager.h"
#include "engine/public/TextureManager/TexManager.h"

void MaterialInstance::Apply(
	UINT                            rootParameterIndex,
	SrvManager*                     srvManager,
	const std::vector<std::string>& textureOrder) {
	// デバッグ情報
	Console::Print("MaterialInstance::Apply 開始\n", kConTextColorCompleted, Channel::ResourceSystem);

	// テクスチャ適用のデバッグ情報
	Console::Print(std::format(
		"MaterialInstance::Apply - ルートパラメータ: {}, テクスチャ数: {}\n",
		rootParameterIndex, textureOrder.size()), 
		kConTextColorCompleted, Channel::ResourceSystem);

	// 各スロットに対応するテクスチャパスを取得し、適用する
	for (size_t i = 0; i < textureOrder.size(); ++i) {
		const auto& slotName = textureOrder[i];

		// スロット名に対応するテクスチャパスを取得
		std::string texturePath;
		auto        it = textureSlots_.find(slotName);
		if (it != textureSlots_.end() && !it->second.empty()) {
			texturePath = it->second;
			Console::Print(std::format(
				               "MaterialInstance::Apply: スロット {} のテクスチャパス: {}\n",
				               slotName, texturePath), 
				               kConTextColorCompleted, Channel::ResourceSystem);
		} else {
			// スロットに対応するテクスチャが見つからない場合、スロット名自体をパスとして使用
			texturePath = slotName;
			Console::Print(std::format(
				               "MaterialInstance::Apply: スロット {} にテクスチャが未設定、スロット名をパスとして使用: {}\n",
				               slotName, texturePath), 
				               kConTextColorWarning, Channel::ResourceSystem);
		}

		// テクスチャパスの検証
		if (texturePath.empty()) {
			Console::Print("MaterialInstance::Apply: テクスチャパスが空です\n",
			               kConTextColorError, Channel::ResourceSystem);
			continue;
		}

		// 特定のテクスチャパスの確認（デバッグ用）
		if (texturePath.find("dev_measure.png") != std::string::npos ||
			texturePath.find("smoke.png") != std::string::npos) {
			Console::Print(
				std::format("MaterialInstance::Apply: 追跡中のテクスチャ: {} をバインドします\n",
				            texturePath),
				kConTextColorWarning, Channel::ResourceSystem);
		}
		
		TexManager* texManager = TexManager::GetInstance();

		// テクスチャをロード確認
		texManager->LoadTexture(texturePath);

		// テクスチャのインデックスを取得
		uint32_t idx = texManager->GetTextureIndexByFilePath(texturePath);
		
		// テクスチャのGPUハンドルを直接取得
		D3D12_GPU_DESCRIPTOR_HANDLE handle = texManager->GetSrvHandleGPU(texturePath);
		
		// GPUハンドルが無効な場合はインデックスでバインド
		if (handle.ptr == 0) {
			// バインディング情報をログ出力
			Console::Print(std::format(
							   "[MaterialInstance] テクスチャ {} (インデックス {}) をルートパラメータ {} にバインド\n",
							   texturePath, idx, rootParameterIndex),
						   kConTextColorWarning, Channel::ResourceSystem);
						   
			// インデックスが0の場合は警告
			if (idx == 0) {
				Console::Print(std::format(
								   "[警告] テクスチャ {} のインデックスが0（デフォルト）です！\n",
								   texturePath),
							   kConTextColorError, Channel::ResourceSystem);
			}
		} else {
			// バインディング情報をログ出力
			Console::Print(std::format(
							   "[MaterialInstance] テクスチャ {} (ハンドル {}) をルートパラメータ {} に直接バインド\n",
							   texturePath, handle.ptr, rootParameterIndex),
						   kConTextColorCompleted, Channel::ResourceSystem);
		}
		
		// SRVをバインド - インデックスが有効であることを確認
		if (idx > 0 || handle.ptr != 0) {
			srvManager->SetGraphicsRootDescriptorTable(rootParameterIndex, idx);
			
			Console::Print(std::format(
							   "[MaterialInstance] テクスチャ {} が正常にバインドされました (index: {})\n",
							   texturePath, idx),
						   kConTextColorCompleted, Channel::ResourceSystem);
		} else {
			Console::Print(std::format(
							   "[エラー] テクスチャ {} のバインドに失敗しました (index: {})\n",
							   texturePath, idx),
						   kConTextColorError, Channel::ResourceSystem);
		}
	}
}
