#pragma once
#include <memory>

#include <ResourceSystem/Material/Material.h>

class MaterialManager {
public:
	Material* GetOrCreateMaterial(const std::string& name, Shader* shader);
	Material* GetOrCreateMaterial(const std::string& name, Shader* shader, const std::string& meshName);
	Material* GetMaterial(const std::string& name);
	Material* GetMaterial(const std::string& name, const std::string& meshName);
	void Init();
	void Shutdown();
private:
	std::string GenerateMaterialKey(const std::string& materialName, const std::string& meshName = "") const;
	std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

