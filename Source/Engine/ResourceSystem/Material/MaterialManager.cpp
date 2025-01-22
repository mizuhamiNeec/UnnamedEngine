#include "MaterialManager.h"

#include "Lib/Console/Console.h"

#include "ResourceSystem/Pipeline/PipelineManager.h"

Material* MaterialManager::GetOrCreateMaterial(const std::string& name, Shader* shader) {
	// 既に作成済みのマテリアルがあるか確認
	if (materials_.contains(name)) {
		return materials_[name].get();
	}

	// なかったので作成
	auto material = std::make_unique<Material>(name, shader);
	materials_[name] = std::move(material);
	return materials_[name].get();
}

Material* MaterialManager::GetMaterial(const std::string& name) {
	auto it = materials_.find(name);
	return it != materials_.end() ? it->second.get() : nullptr;
}

void MaterialManager::Init() {
	Console::Print(
		"MaterialManager を初期化しています...\n",
		kConsoleColorGray,
		Channel::ResourceSystem
	);
	materials_.clear();
}

void MaterialManager::Shutdown() {
	materials_.clear();
	PipelineManager::Shutdown();
}
