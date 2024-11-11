#pragma once
#include <string>
#include <vector>

#include "../Lib/Math/Vector/Vec3.h"

class LevelData {
public:
	struct ObjectData {
		std::string fileName;
		Vec3 position;
		Vec3 rotation;
		Vec3 scale;
		std::string colliderType;
		Vec3 colliderCenter;
		Vec3 colliderSize;
	};

	std::vector<ObjectData> objects; // レベル内のオブジェクトデータのリスト

	LevelData() = default;
	~LevelData() = default;
};
