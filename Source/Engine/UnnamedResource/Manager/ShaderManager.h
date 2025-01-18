#pragma once
#include <memory>

#include <UnnamedResource/Shader.h>

class ShaderManager {
public:
	static Shader* LoadShader(const std::string& vsPath, const std::string& psPath, const std::string& gsPath);
	static Shader* GetShader(const std::string& name);
	static void Init();
	static void Shutdown();
private:
	static std::unordered_map<std::string, std::unique_ptr<Shader>> shaders_;
};

