#pragma once
#include <string>

class IConVar {
public:
	virtual ~IConVar() = default;

	[[nodiscard]] virtual std::string GetValueAsString() const = 0;
	[[nodiscard]] virtual float GetValueAsFloat() const = 0;
	[[nodiscard]] virtual double GetValueAsDouble() const = 0;
	[[nodiscard]] virtual int GetValueAsInt() const = 0;
	[[nodiscard]] virtual bool GetValueAsBool() const = 0;
	[[nodiscard]] virtual Vec3 GetValueAsVec3() const = 0;
	[[nodiscard]] virtual std::string GetTypeAsString() const = 0;
	[[nodiscard]] virtual const std::string& GetName() const = 0;
	[[nodiscard]] virtual const std::string& GetHelp() const = 0;

	virtual void SetValueFromFloat(const float& newValue) = 0;
	virtual void SetValueFromDouble(const double& newValue) = 0;
	virtual void SetValueFromInt(const int& newValue) = 0;
	virtual void SetValueFromBool(const bool& newValue) = 0;
	virtual void SetValueFromString(const std::string& newValue) = 0;
};
