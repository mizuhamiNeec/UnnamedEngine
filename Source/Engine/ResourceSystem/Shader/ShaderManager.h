#pragma once
#include <memory>

#include <ResourceSystem/Shader/Shader.h>

class ShaderManager {
public:
	Shader* LoadShader(const std::string& name, const std::string& vsPath, const std::string& psPath, const std::string& gsPath = "");
	Shader* GetShader(const std::string& name);
	void Init();
	void Shutdown();
private:
	std::unordered_map<std::string, std::unique_ptr<Shader>> shaders_;
};

