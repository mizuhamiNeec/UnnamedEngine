#include "ConVars.h"

#include "ConVar.h"

void ConVars::AddConVar(ConVar* conVar) {
	conVars_[conVar->GetName()] = conVar;
}

ConVar* ConVars::GetConVar(const std::string& name) {
	if (conVars_.contains(name)) {
		return conVars_[name];
	}
	return nullptr;
}

std::unordered_map<std::string, ConVar*> ConVars::GetAllConVars() const {
	return conVars_;
}

