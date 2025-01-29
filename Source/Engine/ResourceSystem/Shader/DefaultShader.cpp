#include "DefaultShader.h"

#include "ShaderManager.h"

const std::string DefaultShader::kDefaultVSPath = "./Resources/Shaders/Object3d.VS.hlsl";
const std::string DefaultShader::kDefaultPSPath = "./Resources/Shaders/Object3d.PS.hlsl";
const std::string DefaultShader::kDefaultShaderName = "DefaultShader";

Shader* DefaultShader::CreateDefaultShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader("DefaultShader", kDefaultVSPath, kDefaultPSPath);
}
