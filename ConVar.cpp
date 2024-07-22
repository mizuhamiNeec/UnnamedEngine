#include "ConVar.h"

#include <cassert>

ConVar::ConVar(std::string name, int value, std::string description)
	: name_(std::move(name)), type_(ConVarType::INT), description_(std::move(description)), value_(value) {
	ConVars::GetInstance().AddConVar(this);
}

ConVar::ConVar(std::string name, float value, std::string description)
	: name_(std::move(name)), type_(ConVarType::FLOAT), description_(std::move(description)), value_(value) {
	ConVars::GetInstance().AddConVar(this);
}

ConVar::ConVar(std::string name, Vec3 value, std::string description)
	: name_(std::move(name)), type_(ConVarType::VEC3), description_(std::move(description)), value_(value) {
	ConVars::GetInstance().AddConVar(this);
}

int ConVar::GetInt() const {
	if (type_ == ConVarType::INT) {
		return std::get<int>(value_);
	}
	assert(0 && "私、intじゃないです");
	return 0;
}

float ConVar::GetFloat() const {
	if (type_ == ConVarType::FLOAT) {
		return std::get<float>(value_);
	}
	assert(0 && "私、floatじゃないです");
	return 0.0f;
}

Vec3 ConVar::GetVec3() const {
	if (type_ == ConVarType::VEC3) {
		return std::get<Vec3>(value_);
	}
	assert(0 && "私、Vec3じゃないです");
	return Vec3::Zero();
}
