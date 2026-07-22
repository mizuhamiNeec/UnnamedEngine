#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	class World;
	class Entity;
	class BaseComponent;
	class JsonReader;
	class JsonWriter;

	using EntityId = uint64_t;

	struct SceneEntityHandle {
		EntityId id = 0;
	};

	class Scene {
	public:
		Scene();
		~Scene();

		// コピー・コピー代入禁止
		Scene(const Scene&)            = delete;
		Scene& operator=(const Scene&) = delete;

		/// @brief 新しいエンティティをシーンに作成します。
		/// @param name エンティティの名前
		/// @param id
		/// @param isEditorOnly
		/// @return 作成されたエンティティの参照
		[[nodiscard]] Entity& CreateEntity(
			std::string_view name, uint64_t id, bool isEditorOnly
		);

		/// @brief 指定したIDのエンティティを削除予定にします。次フレームで削除されます。
		/// @param id エンティティID
		void DestroyEntity(EntityId id);

		/// @brief 削除予定のエンティティを削除します。
		void ProcessPendingEntityDestruction();

		/// @brief 指定したIDのエンティティをシーン内で検索します。
		/// @param id エンティティID
		/// @return エンティティのポインタ。存在しない場合は nullptr。
		[[nodiscard]] Entity* FindEntity(EntityId id);

		/// @brief 指定したIDのエンティティをシーン内で検索します。(const版)
		/// @param id エンティティID
		/// @return エンティティのポインタ。存在しない場合は nullptr。
		[[nodiscard]] const Entity* FindEntity(EntityId id) const;

		/// @brief シーン内のエンティティ数を取得します。
		[[nodiscard]] size_t GetEntityCount() const;

		/// @brief シーン内のすべてのエンティティを取得します。
		[[nodiscard]]
		const std::vector<std::unique_ptr<Entity>>&   GetEntities() const;
		[[nodiscard]] const std::vector<std::string>& GetFolders() const;

		/// @brief シーンが所属するワールドを取得します。
		/// @return ワールドへのポインタ。シーンがワールドに所属していない場合は nullptr。
		[[nodiscard]] World* GetWorld() const noexcept;

		/// @brief シーンが所属するワールドを設定します。
		/// @param world ワールドへのポインタ
		void SetWorld(World* world) noexcept;

		void AddFolder(std::string_view folderPath);
		void RemoveFolder(std::string_view folderPath);
		void RenameFolderSubtree(
			std::string_view sourceFolderPath, std::string_view newLeafName
		);
		void DeleteFolderSubtree(std::string_view folderPath);
		void MoveFolderSubtree(
			std::string_view sourceFolderPath, std::string_view targetParentPath
		);

		/// @brief シーンをシリアライズします。
		/// @param writer JSONライター
		static void Serialize(const JsonWriter& writer);

		/// @brief シーンをデシリアライズします。
		/// @param reader JSONリーダー
		static void Deserialize(const JsonReader& reader);

		/// @brief シーンがロードされた後に呼び出されます。
		void OnPostLoad();

		/// @brief シーンの全エンティティを削除して初期状態に戻します。
		void Reset();

		/// @brief エンティティIDを割り当てます。
		/// @return 新しいエンティティID
		[[nodiscard]] EntityId AllocateEntityId();

	private:
		EntityId                              mNextEntityId = 1;
		World*                                mWorld        = nullptr;
		std::vector<std::unique_ptr<Entity>>  mEntities;
		std::unordered_map<EntityId, Entity*> mEntityById;
		std::vector<std::string>              mFolders;
	};
}
