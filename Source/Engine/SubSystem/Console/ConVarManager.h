#pragma once
#include <mutex>

#include "ConVar.h"
#include "ConVarCache.h"

class ConVarManager {
public:
	template <typename T>
	static void RegisterConVar(
		const std::string& name,
		const T& defaultValue,
		const std::string& helpString = "",
		ConVarFlags flags = ConVarFlags::ConVarFlags_None,
		bool bMin = false,
		float fMin = 0.0f,
		bool bMax = false,
		float fMax = 0.0f
	);

	template <typename T>
	static T GetConVarValue(const std::string& name);

	static IConVar* GetConVar(const std::string& name);
	static void ToggleConVar(const std::string& name);

	static std::vector<IConVar*> GetAllConVars();

private:
	ConVarManager() = default;
	static std::unordered_map<std::string, std::unique_ptr<IConVar>> conVars_;
	static std::mutex mutex_;
};

template <typename T>
void ConVarManager::RegisterConVar(
	const std::string& name,
	const T& defaultValue,
	const std::string& helpString,
	ConVarFlags flags,
	bool bMin,
	float fMin,
	bool bMax,
	float fMax
) {
	std::lock_guard lock(mutex_);
	auto conVar = std::make_unique<ConVar<T>>(name, defaultValue, helpString, flags, bMin, fMin, bMax, fMax);

	ConVarCache::GetInstance().CacheConVar(name, conVar.get());
	conVars_[name] = std::move(conVar);
}

template <typename T>
T ConVarManager::GetConVarValue(const std::string& name) {
	std::lock_guard lock(mutex_);
	auto it = conVars_.find(name);
	if (it != conVars_.end()) {
		auto* var = dynamic_cast<ConVar<T>*>(it->second.get());
		if (var != nullptr) {
			return var->GetValue();
		}
	}
	Console::Print("ConVar not found: " + name, kConsoleColorError);
	return 0;
}
