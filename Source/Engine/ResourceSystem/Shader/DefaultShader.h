#pragma once
#include <string>

#include "Shader.h"
#include "ShaderManager.h"

class DefaultShader {
public:
	static const std::string kDefaultVSPath;
	static const std::string kDefaultPSPath;
	static const std::string kDefaultShaderName;

	static Shader* CreateDefaultShader(ShaderManager* shaderManager);
};

