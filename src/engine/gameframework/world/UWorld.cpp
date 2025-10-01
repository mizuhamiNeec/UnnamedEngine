#include "UWorld.h"

#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

#include <engine/gameframework/component/Camera/UCameraComponent.h>
#include <engine/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/gameframework/component/Transform/TransformComponent.h>
#include <engine/gameframework/component/WorldInstance/WorldInstanceComponent.h>

namespace Unnamed {
	UWorld::UWorld(std::string name) : mName(std::move(name)) {
		// JsonWriter writer("./test.json");
		//
		// for (int i = 0; i < 16384; ++i) {
		// 	SpawnEmpty();
		// }
		//
		// writer.BeginObject();
		//
		// writer.Key("name");
		// writer.Write(mName);
		// writer.Key("entities");
		// writer.BeginArray();
		// for (const auto& e : mEntities) {
		// 	writer.BeginObject();
		// 	writer.Key("name");
		// 	writer.Write(e->GetName().data());
		// 	writer.Key("id");
		// 	writer.Write(e->GetId());
		// 	writer.EndObject();
		// }
		// writer.EndArray();
		//
		// writer.EndObject();
		//
		// writer.Save();
	}

	UWorld::~UWorld() = default;

	UEntity* UWorld::SpawnEmpty(const std::string& name) {
		auto  e   = std::make_unique<UEntity>(name);
		auto* ptr = e.get();
		mEntities.emplace_back(std::move(e));
		return ptr;
	}

	void UWorld::DestroyEntity(uint64_t entityID) {
		std::erase_if(
			mEntities,
			[&](auto& e) { return e && e->GetId() == entityID; }
		);
	}

	void UWorld::Tick(const float deltaTime) {
		for (const auto& e : mEntities) {
			if (!e) { continue; }

			e->PrePhysicsTick(deltaTime);
			e->Tick(deltaTime);
			e->PostPhysicsTick(deltaTime);

			for (auto& [world, parentTransform] : mChildren) {
				if (world) {
					world->Tick(deltaTime);
				}
			}
		}
	}

	void UWorld::PreRender() {
		for (auto& e : mEntities) { if (e) { e->OnPreRender(); } }
		for (auto& cw : mChildren) if (cw.world) { cw.world->PreRender(); }
	}

	void UWorld::PostRender() {
		for (auto& e : mEntities) { if (e) { e->OnPostRender(); } }
		for (auto& cw : mChildren) if (cw.world) { cw.world->PostRender(); }
	}

	bool UWorld::SaveToJson(const std::string& path) {
		JsonWriter w(path);
		w.BeginObject();
		w.Key("name");
		w.Write(mName);
		w.Key("settings");
		{
			w.BeginObject();
			w.Key("worldbounds");
			const auto bounds = Settings().worldBounds;
			w.Write(std::vector{bounds.x, bounds.y, bounds.z});
			w.EndObject();
		}
		w.Key("entities");
		{
			w.BeginArray();
			for (auto& e : mEntities) {
				if (!e) { continue; }
				w.BeginObject();
				w.Key("name");
				w.Write(e->GetName());
				w.Key("components");
				w.BeginArray();
				{
					for (const auto& c : e->GetComponents()) {
						if (!c) continue;
						w.BeginObject();
						w.Key("type");
						w.Write(c->GetComponentName());
						c->Serialize(w);
						w.EndObject();
					}
				}
				w.EndArray();
				w.EndObject();
			}
			w.EndArray();
		}
		if (!mChildren.empty()) {
			w.Key("children");
			w.BeginArray();
			for (auto& cw : mChildren) {
				if (!cw.world) { continue; }
				w.BeginObject();
				w.Key("parentEntity");
				w.Write(
					cw.parentTransform ?
						cw.parentTransform->GetOwner()->GetName() :
						""
				);
				w.EndObject();
			}
			w.EndArray();
		}
		w.EndObject();
		return w.Save();
	}

	bool UWorld::LoadFromJson(
		const std::string& path, UAssetManager*
	) {
		const JsonReader r(path);
		if (!r.Valid()) { return false; }
		if (r.Has("name")) { mName = r["name"].GetString(); }

		if (r.Has("settings")) {
			const auto s = r["settings"];
			if (s.Has("worldbounds")) {
				const auto v          = s["worldbounds"].GetArray();
				mSettings.worldBounds = {
					v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat()
				};
			}
		}

		mEntities.clear();
		if (r.Has("entities")) {
			const auto ents = r["entities"];
			for (size_t i = 0; i < ents.Size(); ++i) {
				auto  eobj = ents[i];
				auto* e    = SpawnEmpty(
					eobj.Has("name") ?
						eobj["name"].GetString() :
						"Entity"
				);
				// とりあえずTransformをモグモグさせる
				auto* t = e->GetOrAddComponent<TransformComponent>();
				if (eobj.Has("components")) {
					auto cs = eobj["components"];
					for (size_t k = 0; k < cs.Size(); ++k) {
						auto           cobj = cs[k];
						std::string    type = cobj["type"].GetString();
						BaseComponent* comp = nullptr;
						if (type == "Transform") {
							comp = t;
						} else if (type == "MeshRenderer") {
							comp =
								e->GetOrAddComponent<MeshRendererComponent>();
						} else if (type == "Camera") {
							comp =
								e->GetOrAddComponent<UCameraComponent>();
						} else if (type == "WorldInstance") {
							comp =
								e->GetOrAddComponent<WorldInstanceComponent>();
						}
						if (comp) { comp->Deserialize(cobj); }
					}
				}
			}
		}
		return true;
	}

	UCameraComponent* UWorld::MainCamera() {
		for (const auto& e : mEntities) {
			if (!e) { continue; }
			if (auto* cam = e->GetComponent<UCameraComponent>()) { return cam; }
		}
		for (auto& [world, parentTransform] : mChildren) {
			if (!world) { continue; }
			if (auto* cam = world->MainCamera()) { return cam; }
		}
		return nullptr;
	}

	void UWorld::AddChildWorld(
		std::unique_ptr<UWorld> sub, TransformComponent* parentTransform
	) {
		mChildren.emplace_back(ChildWorld{
			.world = std::move(sub), .parentTransform = parentTransform
		});
	}
}
