#include "Scene.h"

#include <algorithm>
#include <string>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/scene/SceneFolderPath.h"

namespace Unnamed {
	Scene::Scene()  = default;
	Scene::~Scene() = default;

	Entity& Scene::CreateEntity(
		const std::string_view name, uint64_t id, bool isEditorOnly
	) {
		if (id == 0 || mEntityById.contains(id)) {
			id = AllocateEntityId();
		}

		auto entity = std::make_unique<Entity>(
			std::string(name), id, isEditorOnly
		);
		Entity* raw = entity.get();
		raw->SetScene(this);

		mEntities.emplace_back(std::move(entity));
		mEntityById[id] = raw;
		mNextEntityId   = std::max<EntityId>(mNextEntityId, id + 1);
		return *raw;
	}

	void Scene::DestroyEntity(const EntityId id) {
		const auto it = mEntityById.find(id);
		if (it == mEntityById.end()) {
			return;
		}

		Entity* target = it->second;

		target->MarkPendingDestroy();
	}

	void Scene::ProcessPendingEntityDestruction() {
		for (auto it = mEntities.begin(); it != mEntities.end();) {
			if (!*it || !(*it)->IsPendingDestroy()) {
				++it;
				continue;
			}

			Entity* target = it->get();
			target->OnDestroy();

			mEntityById.erase(target->GetGuid());
			it = mEntities.erase(it);
		}
	}

	Entity* Scene::FindEntity(const EntityId id) {
		const auto it = mEntityById.find(id);
		return it != mEntityById.end() ? it->second : nullptr;
	}

	const Entity* Scene::FindEntity(const EntityId id) const {
		const auto it = mEntityById.find(id);
		return it != mEntityById.end() ? it->second : nullptr;
	}

	size_t Scene::GetEntityCount() const {
		return mEntities.size();
	}

	const std::vector<std::unique_ptr<Entity>>& Scene::GetEntities() const {
		return mEntities;
	}

	const std::vector<std::string>& Scene::GetFolders() const {
		return mFolders;
	}

	World* Scene::GetWorld() const noexcept {
		return mWorld;
	}

	void Scene::SetWorld(World* world) noexcept {
		mWorld = world;
	}

	void Scene::AddFolder(const std::string_view folderPath) {
		const std::string normalized = SceneFolderPath::Normalize(folderPath);
		if (normalized.empty()) {
			return;
		}
		if (
			std::ranges::find(mFolders, normalized) == mFolders.end()
		) {
			mFolders.emplace_back(normalized);
		}
		std::ranges::sort(mFolders);
	}

	void Scene::RemoveFolder(const std::string_view folderPath) {
		const std::string normalized = SceneFolderPath::Normalize(folderPath);
		std::erase(mFolders, normalized);
	}

	void Scene::RenameFolderSubtree(
		const std::string_view sourceFolderPath,
		const std::string_view newLeafName
	) {
		const std::string source = SceneFolderPath::Normalize(sourceFolderPath);
		const std::string leaf   = SceneFolderPath::Normalize(newLeafName);
		if (source.empty() || leaf.empty()) {
			return;
		}

		const std::string destination = SceneFolderPath::Join(
			SceneFolderPath::ParentPath(source), leaf
		);
		if (destination == source) {
			return;
		}

		for (std::string& folder : mFolders) {
			folder = SceneFolderPath::Normalize(
				SceneFolderPath::ReplacePrefix(folder, source, destination)
			);
		}
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) {
				continue;
			}
			const std::string current(entityPtr->GetFolderPath());
			if (!SceneFolderPath::IsEqualOrDescendant(current, source)) {
				continue;
			}
			entityPtr->SetFolderPath(
				SceneFolderPath::ReplacePrefix(current, source, destination)
			);
		}
		std::ranges::sort(mFolders);
		mFolders.erase(
			std::ranges::unique(mFolders).begin(),
			mFolders.end()
		);
	}

	void Scene::DeleteFolderSubtree(const std::string_view folderPath) {
		const std::string source = SceneFolderPath::Normalize(folderPath);
		if (source.empty()) {
			return;
		}

		for (auto it = mFolders.begin(); it != mFolders.end();) {
			if (SceneFolderPath::IsEqualOrDescendant(*it, source)) {
				it = mFolders.erase(it);
			} else {
				++it;
			}
		}

		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) {
				continue;
			}
			const std::string current(entityPtr->GetFolderPath());
			if (!SceneFolderPath::IsEqualOrDescendant(current, source)) {
				continue;
			}
			entityPtr->SetFolderPath("");
		}
	}

	void Scene::MoveFolderSubtree(
		const std::string_view sourceFolderPath,
		const std::string_view targetParentPath
	) {
		const std::string source = SceneFolderPath::Normalize(sourceFolderPath);
		const std::string targetParent =
			SceneFolderPath::Normalize(targetParentPath);
		if (source.empty()) {
			return;
		}

		const size_t      lastSlash = source.find_last_of('/');
		const std::string leafName  = lastSlash == std::string::npos ?
			                             source :
			                             source.substr(lastSlash + 1);
		const std::string destination = targetParent.empty() ?
			                                leafName :
			                                targetParent + "/" + leafName;

		if (destination == source || SceneFolderPath::IsEqualOrDescendant(
			    targetParent, source
		    )) {
			return;
		}

		for (std::string& folder : mFolders) {
			folder = SceneFolderPath::Normalize(
				SceneFolderPath::ReplacePrefix(folder, source, destination)
			);
		}
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) {
				continue;
			}
			const std::string current(entityPtr->GetFolderPath());
			if (!SceneFolderPath::IsEqualOrDescendant(current, source)) {
				continue;
			}
			entityPtr->SetFolderPath(
				SceneFolderPath::ReplacePrefix(current, source, destination)
			);
		}

		std::ranges::sort(mFolders);
		mFolders.erase(
			std::ranges::unique(mFolders).begin(),
			mFolders.end()
		);
	}

	void Scene::Serialize(const JsonWriter& writer) {
		writer; // 未実装
	}

	void Scene::Deserialize(const JsonReader& reader) {
		reader; // 未実装
	}

	void Scene::OnPostLoad() {
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) {
				continue;
			}
			if (auto* transform = entityPtr->GetComponent<
				TransformComponent>()) {
				transform->ResolveDeferredParent(*this);
			}
			AddFolder(entityPtr->GetFolderPath());
		}
	}

	void Scene::Reset() {
		mEntities.clear();
		mEntityById.clear();
		mFolders.clear();
		mNextEntityId = 1;
	}

	EntityId Scene::AllocateEntityId() {
		while (mEntityById.contains(mNextEntityId) || mNextEntityId == 0) {
			++mNextEntityId;
		}
		return mNextEntityId++;
	}
}
