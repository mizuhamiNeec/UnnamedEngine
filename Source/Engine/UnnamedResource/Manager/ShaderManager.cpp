#include "ShaderManager.h"

#include "Lib/Console/Console.h"

Shader* ShaderManager::LoadShader(const std::string& vsPath, const std::string& psPath, const std::string& gsPath) {
	// パスを結合してキーにする
	std::string filePath = vsPath + ";" + psPath + ";" + gsPath;

	// 既に読み込まれているシェーダがあればそれを返す
	if (shaders_.contains(filePath)) {
		return shaders_[filePath].get();
	}

	// 新しいシェーダを作成
	auto shader = std::make_unique<Shader>(vsPath, psPath, gsPath);
	if (shader) {
		// シェーダの読み込みに成功したので登録
		shaders_[filePath] = std::move(shader);
		return shaders_[filePath].get();
	}

	Console::Print(
		"シェーダの読み込みに失敗しました: " + filePath + "\n",
		kConsoleColorError,
		Channel::ResourceSystem
	);

	return nullptr;
}

Shader* ShaderManager::GetShader(const std::string& name) {
	auto it = shaders_.find(name);
	return it != shaders_.end() ? it->second.get() : nullptr;
}

void ShaderManager::Init() {
	shaders_.clear();
}

void ShaderManager::Shutdown() {
	shaders_.clear();
}

std::unordered_map<std::string, std::unique_ptr<Shader>> ShaderManager::shaders_;