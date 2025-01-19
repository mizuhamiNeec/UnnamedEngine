#include "DefaultShader.h"

#include "ShaderManager.h"

const std::string DefaultShader::kDefaultVSPath = "./Resources/Shaders/Default.VS.hlsl";
const std::string DefaultShader::kDefaultPSPath = "./Resources/Shaders/Default.PS.hlsl";
const std::string DefaultShader::kDefaultShaderName = "DefaultShader";

Shader* DefaultShader::CreateDefaultShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader(kDefaultVSPath, kDefaultPSPath);
}
