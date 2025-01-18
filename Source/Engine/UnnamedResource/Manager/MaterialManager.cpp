#include "MaterialManager.h"

Material* MaterialManager::CreateMaterial(const std::string& name, Shader* shader) {
	if (materials_.contains(name)) {
		return materials_[name].get();
	}

	auto material = std::make_unique<Material>(name, shader);
	materials_[name] = std::move(material);
	return materials_[name].get();
}

Material* MaterialManager::GetMaterial(const std::string& name) {
	auto it = materials_.find(name);
	return it != materials_.end() ? it->second.get() : nullptr;
}

void MaterialManager::Init() {
	materials_.clear();
}

void MaterialManager::Shutdown() {
	materials_.clear();
}

std::unordered_map<std::string, std::unique_ptr<Material>> MaterialManager::materials_;