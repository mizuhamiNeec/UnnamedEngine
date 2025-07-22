
#pragma once
#include <math/public/MathLib.h>
#include "math/public/Quaternion.h"

#include <json.hpp>

class ResourceManager;
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

	static void ApplyTransform(const Entity* e, const Transform& t);

	static Entity* LoadNode(Entity* parent, const json& j, BaseScene* scene,
							ResourceManager*
							resourceManager);

	void LoadScene(const std::string& filePath, BaseScene* scene,
				   ResourceManager*   resourceManager);

	void        SaveScene(const std::string& path, BaseScene* scene) const;
	static json SaveNode(Entity* e);

	std::vector<Entity*> loaded_entities;
};
