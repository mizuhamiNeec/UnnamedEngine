#pragma once
#include <memory>

#include <UnnamedResource/Material.h>

class MaterialManager {
public:
	static Material* CreateMaterial(const std::string& name, Shader* shader);
	static Material* GetMaterial(const std::string& name);
	static void Init();
	static void Shutdown();

private:
	static std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

