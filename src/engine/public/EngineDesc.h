#pragma once
#include <string>

/// @struct EngineDesc
/// @brief エンジンの基本情報を格納する構造体
struct EngineDesc {
	void*       windowHandle;
	std::string name;
	std::string version;

	~EngineDesc() {
		windowHandle = nullptr; 
	}
};
