#include <engine/ResourceSystem/Shader/DefaultShader.h>
#include <engine/ResourceSystem/Shader/Shader.h>
#include <engine/ResourceSystem/Shader/ShaderManager.h>

const std::string DefaultShader::kDefaultVSPath =
	"./content/core/shaders/Object3d.VS.hlsl";
const std::string DefaultShader::kDefaultPSPath =
	"./content/core/shaders/Object3d.PS.hlsl";
const std::string DefaultShader::kDefaultShaderName = "DefaultShader";

const std::string DefaultShader::kDefaultSkinnedVSPath =
	"./content/core/shaders/SkinnedObject3d.VS.hlsl";
const std::string DefaultShader::kDefaultSkinnedPSPath =
	"./content/core/shaders/SkinnedObject3d.PS.hlsl";
const std::string DefaultShader::kDefaultSkinnedShaderName =
	"DefaultSkinnedShader";

/// @brief デフォルトシェーダーを作成します
/// @param shaderManager シェーダーマネージャーへのポインタ
/// @return シェーダーポインタ
Shader* DefaultShader::CreateDefaultShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader(kDefaultShaderName, kDefaultVSPath,
	                                 kDefaultPSPath);
}

/// @brief デフォルトスキンドシェーダーを作成します
/// @param shaderManager シェーダーマネージャーへのポインタ
/// @return シェーダーポインタ
Shader* DefaultShader::
CreateDefaultSkinnedShader(ShaderManager* shaderManager) {
	return shaderManager->LoadShader(
		kDefaultSkinnedShaderName,
		kDefaultSkinnedVSPath,
		kDefaultSkinnedPSPath
	);
}
