#pragma once
#include <string>
#include <unordered_map>

#include "IConVar.h"

class ConVarCache {
public:
	static ConVarCache& GetInstance() {
		static ConVarCache instance;
		return instance;
	}

	void CacheConVar(const std::string& name, IConVar* conVar) {
		cachedConVars_[name] = conVar;
	}

	IConVar* GetCachedConVar(const std::string& name) {
		auto it = cachedConVars_.find(name);
		return it != cachedConVars_.end() ? it->second : nullptr;
	}

private:
	std::unordered_map<std::string, IConVar*> cachedConVars_;
};
