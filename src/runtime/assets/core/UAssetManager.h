#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>
#include <variant>

#include <runtime/assets/core/UAsset.h>

namespace Unnamed {
	struct ShaderAssetData;
	struct TextureAssetData;
	struct MaterialAssetData;

	class IAssetLoader;

	/// @class UAssetManager
	/// @brief アセットの管理を行うクラス
	class UAssetManager {
	public:
		using AssetID        = uint32_t;
		using ReloadCallback = std::function<void(AssetID id)>;

		UAssetManager();
		void RegisterLoader(std::unique_ptr<IAssetLoader> loader);

		AssetID LoadFromFile(
			const std::string&         path,
			std::optional<UASSET_TYPE> typeOpt = std::nullopt
		);

		template <class T>
		AssetID CreateRuntimeAsset(
			UASSET_TYPE                 type,
			std::string                 name,
			T&&                         payload,
			const std::vector<AssetID>& dependencies = {}
		);

		void AddRef(AssetID id);
		void Release(AssetID id);

		void SetDependencies(
			AssetID                     id,
			const std::vector<AssetID>& dependencies
		);

		const AssetMetaData& Meta(AssetID id) const;
		template <class T>
		const T* Get(AssetID id) const;

		const std::vector<AssetID>& Dependencies(AssetID id) const;
		const std::vector<AssetID>& Dependents(AssetID id) const;

		bool Reload(AssetID id);
		void SubscribeReload(ReloadCallback callback);

		size_t UnloadUnused();

		AssetID FindByPath(std::string_view path) const;
		AssetID FindByName(std::string_view name) const;

		std::vector<AssetID> AllAssets() const;

	private:
		struct Node {
			AssetMetaData        meta;
			AssetPayload         payload;
			std::vector<AssetID> dependencies;
			std::vector<AssetID> dependents;
		};

		AssetID AllocateID();
		AssetID FindOrCreateSlotByPath(const std::string& path,
		                               UASSET_TYPE        type);

		void RebuildDependents(AssetID id);
		void RebuildAllDependents();

		static std::string NormalizePath(std::string path);

	private:
		mutable std::recursive_mutex mMutex;
		std::vector<Node>            mNodes;
		AssetID                      mNextID = 1;

		std::unordered_map<std::string, AssetID> mPathToID;
		std::unordered_map<std::string, AssetID> mNameToID;

		std::vector<std::unique_ptr<IAssetLoader>> mLoaders;
		std::vector<ReloadCallback>                mReloadCallbacks;
	};
}
