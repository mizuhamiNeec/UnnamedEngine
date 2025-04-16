#pragma once
#include <string>

class Asset {
public:
	virtual ~Asset() = default;
	[[nodiscard]] virtual const std::string& GetID() const = 0;
};

