#pragma once
#include <string>
#include <variant>

#include "ConVars.h"
#include "Source/Engine/Lib/Math/Mathlib.h"
#include "Source/Engine/Lib/Math/Vector/Vec3.h"

class ConVar {
public:
	ConVar(std::string name, int value, std::string description);
	ConVar(std::string name, float value, std::string description);
	ConVar(std::string name, Vec3 value, std::string description);

	const std::string& GetName() const { return name_; }
	ConVarType GetType() const { return type_; }
	const std::string& GetDescription() const { return description_; }

	int GetInt() const;
	float GetFloat() const;
	Vec3 GetVec3() const;

	void SetValue(int value);
	void SetValue(float value);
	void SetValue(Vec3 value);

private:
	std::string name_;
	ConVarType type_;
	std::string description_;
	std::variant<int, float, Vec3> value_;
};
