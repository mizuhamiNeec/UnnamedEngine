#pragma once
#include <string>
#include <unordered_map>
#include <variant>

#include "Source/Engine/Lib/Math/Vector/Vec3.h"

class ConVar;

class ConVars {
public:
	static ConVars& GetInstance() {
		static ConVars instance;
		return instance;
	}

	void AddConVar(ConVar* conVar);
	ConVar* GetConVar(const std::string& name);

	std::vector<ConVar*> GetAllConVars() const;

private:
	std::unordered_map<std::string, ConVar*> conVars_;
};

enum class ConVarType {
	INT,
	FLOAT,
	VEC3
};