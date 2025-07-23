#pragma once

#include <string_view>

class IConsole {
public:
	virtual ~IConsole() = default;

	virtual void Msg(std::string_view message) = 0;
};
