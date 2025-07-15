#include "MaterialManager.h"

#include <format>
#include "SubSystem/Console/Console.h"

#include "ResourceSystem/Pipeline/PipelineManager.h"

Material* MaterialManager::GetOrCreateMaterial(const std::string& name,
                                               Shader*            shader) {
	return GetOrCreateMaterial(name, shader, "");
}

Material* MaterialManager::GetOrCreateMaterial(const std::string& name,
                                               Shader*            shader,
                                               const std::string& meshName) {
	// キーを生成: メッシュ名がある場合は「メッシュ名_マテリアル名」、ない場合はマテリアル名のみ
	std::string key = GenerateMaterialKey(name, meshName);
	
	// 既に作成済みのマテリアルがあるか確認
	if (materials_.contains(key)) {
		Console::Print(
			std::format("MaterialManager: 既存マテリアル再利用 {} (キー: {})\n", name, key),
			kConTextColorCompleted,
			Channel::ResourceSystem
		);
		return materials_[key].get();
	}

	// なかったので作成
	auto material = std::make_unique<Material>(name, shader);
	
	// メッシュ名が指定されている場合は設定
	if (!meshName.empty()) {
		material->SetMeshName(meshName);
	}
	
	materials_[key] = std::move(material);
	
	Console::Print(
		std::format("MaterialManager: 新規マテリアル作成 {} (キー: {})\n", name, key),
		kConTextColorCompleted,
		Channel::ResourceSystem
	);
	
	return materials_[key].get();
}

Material* MaterialManager::GetMaterial(const std::string& name) {
	return GetMaterial(name, "");
}

Material* MaterialManager::GetMaterial(const std::string& name, const std::string& meshName) {
	std::string key = GenerateMaterialKey(name, meshName);
	auto it = materials_.find(key);
	return it != materials_.end() ? it->second.get() : nullptr;
}

std::string MaterialManager::GenerateMaterialKey(const std::string& materialName, const std::string& meshName) const {
	if (meshName.empty()) {
		return materialName;
	}
	return meshName + "_" + materialName;
}

void MaterialManager::Init() {
	Console::Print(
		"MaterialManager を初期化しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);
	materials_.clear();
}

void MaterialManager::Shutdown() {
	for (auto& material : materials_) {
		Console::Print(
			"MatMgr: Releasing " + material.second->GetName() + "\n",
			Vec4::white,
			Channel::ResourceSystem
		);
		material.second->Shutdown();
	}

	PipelineManager::Shutdown();
	materials_.clear();
}
