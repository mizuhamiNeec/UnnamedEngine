#pragma once
#include <mutex>

#include "ConVar.h"
#include "ConVarCache.h"

class ConVarManager {
public:
	static ConVarManager& GetInstance() {
		static ConVarManager instance;
		return instance;
	}

	template <typename T>
	void RegisterConVar(
		const std::string& name,
		const T& defaultValue,
		const std::string& helpString,
		ConVarFlags flags = ConVarFlags::ConVarFlags_None,
		bool bMin = false,
		float fMin = 0.0f,
		bool bMax = false,
		float fMax = 0.0f
	) {
		std::lock_guard lock(mutex_);
		auto conVar = std::make_unique<ConVar<T>>(name, defaultValue, helpString, flags, bMin, fMin, bMax, fMax);

		ConVarCache::GetInstance().CacheConVar(name, conVar.get());
		conVars_[name] = std::move(conVar);
	}

	template <typename T>
	T GetConVarValue(const std::string& name) {
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

	IConVar* GetConVar(const std::string& name) {
		std::lock_guard lock(mutex_);
		auto it = conVars_.find(name);
		return it != conVars_.end() ? it->second.get() : nullptr;
	}

	std::vector<IConVar*> GetAllConVars() {
		std::lock_guard lock(mutex_);
		std::vector<IConVar*> conVarArray;
		conVarArray.reserve(conVars_.size());
		for (const auto& pair : conVars_) {
			conVarArray.push_back(pair.second.get());
		}
		return conVarArray;
	}

private:
	ConVarManager() = default;
	std::unordered_map<std::string, std::unique_ptr<IConVar>> conVars_;
	std::mutex mutex_;
};
