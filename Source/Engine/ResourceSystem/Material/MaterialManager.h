#pragma once
#include <memory>

#include <ResourceSystem/Material/Material.h>

class MaterialManager {
public:
	Material* GetOrCreateMaterial(const std::string& name, Shader* shader);
	Material* GetMaterial(const std::string& name);
	void Init();
	void Shutdown();
private:
	std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

