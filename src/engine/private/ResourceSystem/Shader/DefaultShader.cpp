#include <engine/public/ResourceSystem/Shader/DefaultShader.h>
#include <engine/public/ResourceSystem/Shader/Shader.h>
#include <engine/public/ResourceSystem/Shader/ShaderManager.h>

const std::string DefaultShader::kDefaultVSPath =
	"./Resources/Shaders/Object3d.VS.hlsl";
const std::string DefaultShader::kDefaultPSPath =
	"./Resources/Shaders/Object3d.PS.hlsl";
const std::string DefaultShader::kDefaultShaderName = "DefaultShader";

const std::string DefaultShader::kDefaultSkinnedVSPath =
	"./Resources/Shaders/SkinnedObject3d.VS.hlsl";
const std::string DefaultShader::kDefaultSkinnedPSPath =
	"./Resources/Shaders/SkinnedObject3d.PS.hlsl";
const std::string DefaultShader::kDefaultSkinnedShaderName =
	"DefaultSkinnedShader";

Shader* DefaultShader::CreateDefaultShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader(kDefaultShaderName, kDefaultVSPath,
	                                 kDefaultPSPath);
}

Shader* DefaultShader::
CreateDefaultSkinnedShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader(
		kDefaultSkinnedShaderName,
		kDefaultSkinnedVSPath,
		kDefaultSkinnedPSPath
	);
}
