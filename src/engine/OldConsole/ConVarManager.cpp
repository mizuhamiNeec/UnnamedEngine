#include "ConVarManager.h"

std::unordered_map<std::string, std::unique_ptr<IConVar>>
ConVarManager::mConVars;
std::mutex ConVarManager::mMutex;

/// @brief コンソール変数を取得します
/// @param name コンソール変数の名前
/// @return コンソール変数のポインタ、存在しない場合はnullptr
IConVar* ConVarManager::GetConVar(const std::string& name) {
	std::lock_guard lock(mMutex);
	auto            it = mConVars.find(name);
	return it != mConVars.end() ? it->second.get() : nullptr;
}

/// @brief コンソール変数の値をトグル（切り替え）します
/// @param name コンソール変数の名前
void ConVarManager::ToggleConVar(const std::string& name) {
	std::lock_guard lock(mMutex);
	auto            it = mConVars.find(name);
	if (it != mConVars.end()) {
		it->second->Toggle();
		return;
	}

	Console::Print(
		"ConVar not found: " + name, kConTextColorError,
		Channel::Console
	);
}

/// @brief 登録されている全てのコンソール変数を取得します
/// @return コンソール変数のポインタのベクター
std::vector<IConVar*> ConVarManager::GetAllConVars() {
	std::lock_guard       lock(mMutex);
	std::vector<IConVar*> conVarArray;
	conVarArray.reserve(mConVars.size());
	for (const auto& pair : mConVars) {
		conVarArray.emplace_back(pair.second.get());
	}
	return conVarArray;
}
