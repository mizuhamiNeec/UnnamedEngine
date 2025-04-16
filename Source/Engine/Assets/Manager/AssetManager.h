#pragma once
#include <mutex>
#include <unordered_map>
#include <Assets/Asset/TextureAsset.h>
#include <Async/JobSystem.h>

#include "SubSystem/Console/Console.h"

class AssetManager {
public:
	explicit AssetManager(JobSystem& jobSystem) : mJobSystem_(jobSystem) {}

	template<typename T>
	std::shared_ptr<T> LoadAsset(const std::string& assetId) {
		{
			// キャッシュに存在している場合はそれを返す
			std::lock_guard lock(mMutex_);
			auto it = mAssetCache_.find(assetId);
			if (it != mAssetCache_.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
		}

		// 非同期でアセットを読み込むタスクを登録
		auto futureAsset = mJobSystem_.SubmitJob(0, [assetId]() -> std::shared_ptr<Asset> {
			return std::make_shared<TextureAsset>(assetId);
			});

		std::shared_ptr<Asset> asset = futureAsset.get();
		{
			std::lock_guard lock(mMutex_);
			mAssetCache_[assetId] = asset;
		}

		return std::static_pointer_cast<T>(asset);
	}

	void PrintCacheStatus() {
		std::lock_guard lock(mMutex_);
		Console::Print("Asset Cache Status:");
		for (const auto& pair : mAssetCache_) {
			Console::Print(" - " + pair.first + "\n");
		}
	}

private:
	JobSystem& mJobSystem_;
	std::unordered_map<std::string, std::shared_ptr<Asset>> mAssetCache_;
	std::mutex mMutex_;
};

