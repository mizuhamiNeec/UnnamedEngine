#pragma once
#include <memory>
#include <string>
#include <vector>

#include <runtime/core/math/Math.h>

#include <engine/public/gameframework/entity/UEntity/UEntity.h>

#include "WorldSettings.h"

namespace Unnamed {
	class UWorld {
	public:
		explicit UWorld(std::string name = "World");
		~UWorld();

		template <class T, class... Args>
		T* SpawnEntity(Args&&... args) {
			auto e = std::make_unique<UEntity>(std::forward<Args>(args)...);
			T*   t = dynamic_cast<T*>(e.get());
			mEntities.emplace_back(std::move(e));
			return t;
		}

		UEntity* SpawnEmpty(const std::string& name = "Entity");
		void     DestroyEntity(uint64_t entityID);

		void Tick(float deltaTime);
		void PreRender();
		void PostRender();

		bool SaveToJson(const std::string& path);
		bool LoadFromJson(
			const std::string& path, class UAssetManager* assetManager
		);

		class UCameraComponent* MainCamera();

		struct ChildWorld {
			std::unique_ptr<UWorld>   world;
			class TransformComponent* parentTransform = nullptr;
		};

		void AddChildWorld(
			std::unique_ptr<UWorld> sub, TransformComponent* parentTransform
		);

		[[nodiscard]] const std::vector<ChildWorld>&
		Children() const {
			return mChildren;
		}

		[[nodiscard]] const std::vector<std::unique_ptr<UEntity>>&
		Entities() const {
			return mEntities;
		}

		WorldSettings& Settings() { return mSettings; }

		[[nodiscard]] const WorldSettings& Settings() const {
			return mSettings;
		}

		[[nodiscard]] const std::string& Name() const { return mName; }
		void SetName(std::string& name) { mName = std::move(name); }

	private:
		std::string   mName;
		WorldSettings mSettings;

		std::vector<std::unique_ptr<UEntity>> mEntities;
		std::vector<ChildWorld>               mChildren;
	};
}
