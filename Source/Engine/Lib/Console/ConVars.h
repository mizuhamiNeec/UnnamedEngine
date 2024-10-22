#pragma once
#include <string>
#include <unordered_map>

class ConVar;

enum class ConVarType {
	INT,
	FLOAT,
	VEC3,
};

class ConVars {
public:
	static ConVars& GetInstance() {
		static ConVars instance;
		return instance;
	}

	void AddConVar(ConVar* conVar);
	ConVar* GetConVar(const std::string& name);

	static const char* ToString(const ConVarType e) {
		switch (e) {
		case ConVarType::INT: return "INT";
		case ConVarType::FLOAT: return "FLOAT";
		case ConVarType::VEC3: return "VEC3";
		}
		return "unknown";
	}

	std::unordered_map<std::string, ConVar*> GetAllConVars() const;

private:
	std::unordered_map<std::string, ConVar*> conVars_;
};