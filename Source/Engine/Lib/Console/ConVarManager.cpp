#include "ConVarManager.h"

std::unordered_map<std::string, std::unique_ptr<IConVar>> ConVarManager::conVars_;
std::mutex ConVarManager::mutex_;

IConVar* ConVarManager::GetConVar(const std::string& name) {
	std::lock_guard lock(mutex_);
	auto it = conVars_.find(name);
	return it != conVars_.end() ? it->second.get() : nullptr;
}

std::vector<IConVar*> ConVarManager::GetAllConVars() {
	std::lock_guard lock(mutex_);
	std::vector<IConVar*> conVarArray;
	conVarArray.reserve(conVars_.size());
	for (const auto& pair : conVars_) {
		conVarArray.push_back(pair.second.get());
	}
	return conVarArray;
}
