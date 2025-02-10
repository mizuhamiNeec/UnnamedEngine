#pragma once
#include "Base/Asset.h"
#include "SubSystem/Console/Console.h"

class TextureAsset: public Asset {
public:
	TextureAsset(const std::string& id): mId(id) {
		Console::Print("Loading Texture...\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 読み込み時間をシミュレート
	}

	[[nodiscard]] const std::string& GetID() const override {
		return mId;
	}

private:
	std::string mId;
};

