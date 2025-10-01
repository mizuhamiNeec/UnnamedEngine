#include <fstream>

#include <engine/Components/MeshRenderer/StaticMeshRenderer.h>
#include <engine/Entity/Entity.h>
#include <engine/Entity/EntityLoader.h>
#include <engine/ResourceSystem/Manager/ResourceManager.h>

#include "game/scene/base/BaseScene.h"


class StaticMeshRenderer;

EntityLoader::~EntityLoader() {
}

void EntityLoader::ApplyTransform(
	const Entity*    e,
	const Transform& t
) {
	e->GetTransform()->SetLocalPos(t.pos);
	e->GetTransform()->SetLocalRot(t.rot);
	e->GetTransform()->SetLocalScale(t.scale);
}

static Vec3 ToEnginePos(const Vec3& v) {
	return {v.x, v.z, v.y};
}

static Quaternion ToEngineQuat(const Quaternion& q) {
	return {
		q.x,
		-q.z,
		-q.y,
		q.w
	};
}

static Vec3 ToBlenderPos(const Vec3& v) {
	return {v.x, v.z, v.y}; // y成分の符号を修正
}

static Quaternion ToBlenderQuat(const Quaternion& q) {
	return {
		q.x, -q.z, -q.y, q.w // 変更なし (既に正しい逆変換)
	};
}

Entity* EntityLoader::LoadNode(
	Entity*          parent,
	const json&      j,
	BaseScene*       scene,
	ResourceManager* resourceManager
) {
	Entity* e = scene->CreateEntity(j.value("name", "Unnamed"));
	if (parent) {
		parent->AddChild(e);
	}

	const auto& tf = j.at("transform");

	Transform t;

	t.pos = {tf["translation"][0], tf["translation"][1], tf["translation"][2]};
	//t.rot = {tf["rotation"][0], tf["rotation"][1], tf["rotation"][2]};

	Quaternion q = {
		tf["rotation_quaternion"][0],
		tf["rotation_quaternion"][1],
		tf["rotation_quaternion"][2],
		tf["rotation_quaternion"][3]
	};
	//Quaternion qEngine = ToEngineQuat(q);
	t.scale = {tf["scaling"][0], tf["scaling"][1], tf["scaling"][2]};
	t.pos   = ToEnginePos(t.pos);

	t.rot = ToEngineQuat(q);
	ApplyTransform(e, t);

	if (j.contains("file_name")) {
		const auto renderer = e->AddComponent<StaticMeshRenderer>();

		if (
			resourceManager->GetMeshManager()->LoadMeshFromFile(
				j.at("file_name").get<std::string>()
			)
		) {
			// 読めたらメッシュに設定
			renderer->SetStaticMesh(
				resourceManager->GetMeshManager()->GetStaticMesh(
					j.at("file_name").get<std::string>()
				)
			);
		}
	}

	if (j.contains("children")) {
		for (const auto& child : j.at("children")) {
			LoadNode(e, child, scene, resourceManager);
		}
	}

	return e;
}

void EntityLoader::LoadScene(
	const std::string& filePath,
	BaseScene*         scene,
	ResourceManager*   resourceManager
) {
	json root;
	std::ifstream(filePath) >> root;
	for (auto& obj : root.at("objects")) {
		mLoadedEntities.emplace_back(
			LoadNode(nullptr, obj, scene, resourceManager)
		);
	}
}

void EntityLoader::SaveScene(
	const std::string& path,
	BaseScene*         scene
) const {
	json root;
	root["name"]    = "scene";
	root["objects"] = json::array();

	std::vector<Entity*> rootEntities;

	for (auto* e : scene->GetEntities()) {
		if (e->GetParent() == nullptr) {
			if (e->GetName() == "editorCamera") {
				continue;
			}
			rootEntities.emplace_back(e);
		}
	}

	for (auto* e : rootEntities) {
		root["objects"].emplace_back(SaveNode(e));
	}

	std::ofstream(path) << root.dump(4);
}

json EntityLoader::SaveNode(
	Entity* e
) {
	const auto* tf  = e->GetTransform();
	Vec3        pos = ToBlenderPos(tf->GetLocalPos());
	Quaternion  q   = tf->GetLocalRot();
	q               = ToBlenderQuat(q);

	Vec3 rot = q.ToEulerAngles();
	Vec3 scl = tf->GetLocalScale();

	json j;
	j["type"]      = "MESH";
	j["name"]      = e->GetName();
	j["transform"] = {
		{"translation", {pos.x, pos.y, pos.z}},
		{"rotation", {rot.x, rot.y, rot.z}},
		{"rotation_quaternion", {q.x, q.y, q.z, q.w}},
		{"scaling", {scl.x, scl.y, scl.z}}
	};

	if (auto m = e->GetComponent<StaticMeshRenderer>()) {
		if (m->GetStaticMesh()) {
			j["file_name"] = m->GetStaticMesh()->GetName();
		}
	}

	if (!e->GetChildren().empty()) {
		j["children"] = json::array();
		for (auto* child : e->GetChildren()) {
			j["children"].emplace_back(SaveNode(child));
		}
	}
	return j;
}
