#include "ConVars.h"

#include <cassert>
#include <ranges>

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

std::vector<ConVar*> ConVars::GetAllConVars() const {
	std::vector<ConVar*> allConVars;
	allConVars.reserve(conVars_.size());
	for (const auto& val : conVars_ | std::views::values) {
		allConVars.push_back(val);
	}
	return allConVars;
}

