#pragma once
#include "Lib/Math/Vector/Vec3.h"

#include <nlohmann/json.hpp>

#include "ResourceSystem/Manager/ResourceManager.h"
using json = nlohmann::json;

class Entity;
class BaseScene;

class EntityLoader {
public:
	~EntityLoader();

	struct Transform {
		Vec3       pos;
		Quaternion rot;
		Vec3       scale;
	};

	void ApplyTransform(Entity* e, const Transform& t);

	Entity* LoadNode(Entity* parent, const json& j, BaseScene* scene,
	                 ResourceManager*
	                 resourceManager);

	void LoadScene(const std::string& filePath, BaseScene* scene,
	               ResourceManager*   resourceManager);

	void SaveScene(const std::string& path, BaseScene* scene) const;
	json SaveNode(Entity* e) const;

	std::vector<Entity*> loadedEntities_;
};
