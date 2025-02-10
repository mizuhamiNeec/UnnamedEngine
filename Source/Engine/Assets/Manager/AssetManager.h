#pragma once
#include <mutex>
#include <unordered_map>
#include <Assets/Asset/TextureAsset.h>
#include <Async/JobSystem.h>

#include "SubSystem/Console/Console.h"

class AssetManager {
public:
	explicit AssetManager(JobSystem& jobSystem): mJobSystem(jobSystem) {}

	template<typename T>
	std::shared_ptr<T> LoadAsset(const std::string& assetId) {
		{
			// キャッシュに存在している場合はそれを返す
			std::lock_guard lock(mMutex);
			auto it = mAssetCache.find(assetId);
			if(it != mAssetCache.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
		}

		// 非同期でアセットを読み込むタスクを登録
		auto futureAsset = mJobSystem.SubmitJob(0,[assetId]() -> std::shared_ptr<Asset> {
			return std::make_shared<TextureAsset>(assetId);
		});

		std::shared_ptr<Asset> asset = futureAsset.get();
		{
			std::lock_guard lock(mMutex);
			mAssetCache[assetId] = asset;
		}

		return std::static_pointer_cast<T>(asset);
	}

	void PrintCacheStatus() {
		std::lock_guard lock(mMutex);
		Console::Print("Asset Cache Status:");
		for(const auto& pair : mAssetCache) {
			Console::Print(" - " + pair.first + "\n");
		}
	}

private:
	JobSystem& mJobSystem;
	std::unordered_map<std::string,std::shared_ptr<Asset>> mAssetCache;
	std::mutex mMutex;
};

