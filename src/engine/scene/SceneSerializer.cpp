#include "SceneSerializer.h"

#include <chrono>

#include "Scene.h"

#include "core/ComponentRegistry.h"
#include "core/guidgenerator/GuidGenerator.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "SceneSerializer";

	bool SceneSerializer::LoadFromFile(
		Scene& scene, Path path, GuidGenerator& guidGen,
		const SceneLoadOptions& options
	) {
		const JsonReader root(path);
		if (!root.Valid()) {
			Error(kChannel, "Failed to open scene: {}", path);
			return false;
		}
		return Deserialize(scene, root, guidGen, options, path);
	}

	bool SceneSerializer::SaveToFile(
		const Scene& scene, const Path& path
	) {
		JsonWriter writer(path);
		Serialize(scene, writer);
		return writer.Save();
	}

	bool SceneSerializer::Deserialize(
		Scene& scene, const JsonReader& root, GuidGenerator& guidGen,
		const SceneLoadOptions& options, const Path& scenePath
	) {
		scene.Reset();

		const JsonReader folders = root["folders"];
		if (folders.Valid()) {
			for (size_t i = 0; i < folders.Size(); ++i) {
				const JsonReader folder = folders[i];
				if (!folder.Valid()) {
					continue;
				}
				scene.AddFolder(folder.GetString());
			}
		}

		const JsonReader entities = root["entities"];

		// エンティティ配列の検証 
		if (!entities.Valid()) {
			return false;
		}

		// 各エンティティの読み込み
		for (size_t i = 0; i < entities.Size(); ++i) {
			const JsonReader e = entities[i];
			if (!e.Valid()) {
				continue;
			}

			const std::string name = e.ReadStringOr("name", "unnamed");
			const std::string folderPath = e.ReadStringOr("folderPath", "");
			const uint64_t guid = e.ReadUint64Or("guid", 0);
			const bool isEditorOnly = e.ReadBoolOr("isEditorOnly", false);
			const bool entityActive = e.ReadBoolOr("active", true);
			const bool entityVisible = e.ReadBoolOr("visible", true);

			uint64_t finalGuid = guid != 0 ? guid : guidGen.Alloc();
			while (finalGuid == 0 || scene.FindEntity(finalGuid) != nullptr) {
				finalGuid = guidGen.Alloc();
			}

			Entity& entity = scene.CreateEntity(name, finalGuid, isEditorOnly);
			entity.SetActive(entityActive);
			entity.SetVisible(entityVisible);
			entity.SetFolderPath(folderPath);

			const JsonReader comps = e["components"];
			if (!comps.Valid()) {
				continue;
			}

			for (size_t ci = 0; ci < comps.Size(); ++ci) {
				const JsonReader c = comps[ci];
				if (!c.Valid()) {
					continue;
				}

				const std::string type = c.ReadStringOr("type", "");
				if (type.empty()) {
					continue;
				}

				auto comp = ComponentRegistry::Get().Create(type);
				if (!comp) {
					Warning(kChannel, "Unknown component type: {}", type);
					Warning(kChannel, "コンポーネントの登録を忘れている可能性があります。");
					continue;
				}

				const uint64_t compGuid = c.ReadUint64Or("guid", 0);
				comp->SetGuid(compGuid != 0 ? compGuid : guidGen.Alloc());

				comp->SetActive(c.ReadBoolOr("active", true));

				const JsonReader data = c["data"];
				if (data.Valid()) {
					const World* world = scene.GetWorld();
					const SceneDeserializeContext context{
						.loadOptions  = options,
						.assetManager = world ? world->GetAssetManager() : nullptr,
						.scenePath    = scenePath,
						.entityName   = name,
						.entityId     = finalGuid,
						.componentType = type,
					};
					if (!comp->Deserialize(data, context)) {
						Error(
							kChannel,
							"Component deserialize failed: scene='{}' entity='{}' entityId={} component='{}'",
							scenePath,
							name,
							finalGuid,
							type
						);
						scene.Reset();
						return false;
					}
				}

				(void)entity.AddComponentInstance(std::move(comp));
			}
		}

		// シーンのポストロード処理
		scene.OnPostLoad();

		return true;
	}

	void SceneSerializer::Serialize(
		const Scene& scene, JsonWriter& writer
	) {
		writer.BeginObject();

		writer.Key("version");
		writer.Write(1);

		writer.Key("folders");
		writer.BeginArray();
		for (const std::string& folder : scene.GetFolders()) {
			writer.Write(folder);
		}
		writer.EndArray();

		writer.Key("entities");
		writer.BeginArray();

		for (const auto& ePtr : scene.GetEntities()) {
			if (!ePtr) {
				continue;
			}
			const Entity& e = *ePtr;

			writer.BeginObject();

			writer.Key("name");
			writer.Write(std::string(e.GetName()));

			writer.Key("guid");
			writer.Write(e.GetGuid());

			writer.Key("folderPath");
			writer.Write(std::string(e.GetFolderPath()));

			writer.Key("isEditorOnly");
			writer.Write(e.IsEditorOnly());

			writer.Key("active");
			writer.Write(e.IsActive());

			writer.Key("visible");
			writer.Write(e.IsVisible());

			writer.Key("components");
			writer.BeginArray();

			e.ForEachComponent(
				[&writer](const BaseComponent& c) {
					writer.BeginObject();

					writer.Key("type");
					writer.Write(std::string(c.GetStableName()));

					writer.Key("guid");
					writer.Write(c.GetGuid());

					writer.Key("active");
					writer.Write(c.IsActive());

					writer.Key("data");
					writer.BeginObject();
					c.Serialize(writer);
					writer.EndObject();

					writer.EndObject();
				}
			);

			writer.EndArray();
			writer.EndObject();
		}

		writer.EndArray();
		writer.EndObject();
	}

	bool SceneSerializer::CloneScene(
		const Scene& src, Scene& dst, GuidGenerator& guidGen
	) {
		try {
			const auto start = std::chrono::steady_clock::now();
			JsonWriter writer("__clone__.json");
			const auto serializeStart = std::chrono::steady_clock::now();
			Serialize(src, writer);
			const auto serializeEnd = std::chrono::steady_clock::now();

			const auto parseStart = std::chrono::steady_clock::now();
			const auto root = nlohmann::json::parse(writer.ToString());
			const auto parseEnd = std::chrono::steady_clock::now();
			const JsonReader reader(root);
			const auto deserializeStart = std::chrono::steady_clock::now();
			const SceneLoadOptions options{};
			const Path scenePath("__clone__.json");
			const bool ok = Deserialize(
				dst, reader, guidGen, options, scenePath
			);
			const auto deserializeEnd = std::chrono::steady_clock::now();

			DevMsg(
				kChannel,
				"CloneScene timing: serialize={}ms parse={}ms deserialize={}ms total={}ms",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					serializeEnd - serializeStart
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					parseEnd - parseStart
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					deserializeEnd - deserializeStart
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					deserializeEnd - start
				).count()
			);

			return ok;
		} catch (...) {
			Error(kChannel, "Failed to clone scene");
			return false;
		}
	}
}
