#pragma once
#include <string>

class Resource {
public:
	virtual ~Resource() = default;
	virtual void Unload() = 0; // リソースの解放

	[[nodiscard]] const std::string& GetFilePath() const { return filepath_; }

protected:
	std::string filepath_; // ファイルパス
};

