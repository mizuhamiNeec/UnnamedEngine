#pragma once
#include <string>

class IConVar {
public:
	virtual ~IConVar() = default;

	[[nodiscard]] virtual std::string GetValueAsString() const = 0;
	[[nodiscard]] virtual std::string GetTypeAsString() const = 0;
	[[nodiscard]] virtual const std::string& GetName() const = 0;
	[[nodiscard]] virtual const std::string& GetHelp() const = 0;

	virtual void SetValueFromString(const std::string& valueStr) = 0;
};
