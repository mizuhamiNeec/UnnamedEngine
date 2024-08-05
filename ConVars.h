#pragma once
#include <string>
#include <unordered_map>

class ConVar;

class ConVars {
public:
	static ConVars& GetInstance() {
		static ConVars instance;
		return instance;
	}

	void AddConVar(ConVar* conVar);
	ConVar* GetConVar(const std::string& name);

	std::unordered_map<std::string, ConVar*> GetAllConVars() const;

private:
	std::unordered_map<std::string, ConVar*> conVars_;
};

enum class ConVarType {
	INT,
	FLOAT,
	VEC3
};