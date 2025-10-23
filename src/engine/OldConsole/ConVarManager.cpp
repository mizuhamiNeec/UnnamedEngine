#include "ConVarManager.h"

std::unordered_map<std::string, std::unique_ptr<IConVar>>
ConVarManager::conVars_;
std::mutex ConVarManager::mutex_;

/// @brief コンソール変数を取得します
/// @param name コンソール変数の名前
/// @return コンソール変数のポインタ、存在しない場合はnullptr
IConVar* ConVarManager::GetConVar(const std::string& name) {
	std::lock_guard lock(mutex_);
	auto            it = conVars_.find(name);
	return it != conVars_.end() ? it->second.get() : nullptr;
}

/// @brief コンソール変数の値をトグル（切り替え）します
/// @param name コンソール変数の名前
void ConVarManager::ToggleConVar(const std::string& name) {
	auto it = conVars_.find(name);
	if (it != conVars_.end()) {
		it->second->Toggle();
		return;
	}

	Console::Print("ConVar not found: " + name, kConTextColorError,
	               Channel::Console);
}

/// @brief 登録されている全てのコンソール変数を取得します
/// @return コンソール変数のポインタのベクター
std::vector<IConVar*> ConVarManager::GetAllConVars() {
	std::lock_guard       lock(mutex_);
	std::vector<IConVar*> conVarArray;
	conVarArray.reserve(conVars_.size());
	for (const auto& pair : conVars_) {
		conVarArray.emplace_back(pair.second.get());
	}
	return conVarArray;
}
