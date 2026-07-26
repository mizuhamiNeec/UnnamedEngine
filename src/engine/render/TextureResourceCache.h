#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/assets/AssetID.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Render {
	class RgResourceRegistry;

	/// @brief TextureResourceCache のデバッグ統計情報です。
	struct TextureResourceCacheDebugStats {
		uint32_t spriteEntryCount       = 0;
		uint32_t skyboxEntryCount       = 0;
		uint32_t materialEntryCount     = 0;
		uint32_t liveEntryCount         = 0;
		uint64_t createdTextureCount    = 0;
		uint64_t ttlReleaseCount        = 0;
		uint64_t versionRecreateCount   = 0;
		uint64_t releaseAllReleaseCount = 0;
		uint64_t failedResolveCount     = 0;
		uint64_t lastFrameReleasedByTtl = 0;
	};

	/// @brief AssetID から RgTextureId への解決と寿命管理を行います。
	class TextureResourceCache {
	public:
		/// @brief キャッシュを初期化します。
		void Initialize(
			AssetManager* assetManager, RgResourceRegistry* registry
		);

		/// @brief 現在フレーム番号を設定します。
		void BeginFrame(uint64_t frameIndex);

		/// @brief 未使用エントリを解放するまでの猶予フレーム数を設定します。
		void SetUnusedFrameThreshold(uint64_t thresholdFrames);

		/// @brief スプライト用テクスチャを解決します。
		[[nodiscard]] uint32_t ResolveSpriteTexture(AssetID assetId);

		/// @brief スカイボックス用テクスチャを解決します。
		[[nodiscard]] uint32_t ResolveSkyboxTexture(AssetID assetId);

		/// @brief マテリアル用2Dテクスチャを解決します。
		[[nodiscard]] uint32_t ResolveTexture2D(AssetID assetId);

		/// @brief 一定フレーム未使用のテクスチャを解放します。
		void CollectGarbage();

		/// @brief キャッシュ済みテクスチャをすべて解放します。
		void ReleaseAll();

		/// @brief デバッグ統計を返します。
		[[nodiscard]] TextureResourceCacheDebugStats GetDebugStats() const;

	private:
		/// @brief CacheEntryは、レンダリングキャッシュ内の資源と最終利用情報を同じ寿命で保持します
		struct CacheEntry {
			uint32_t textureId     = 0;
			uint32_t assetVersion  = 0;
			uint64_t lastUsedFrame = 0;
		};

		[[nodiscard]] uint32_t ResolveTexture(
			AssetID                                  assetId,
			bool                                     requireCubeMap,
			bool                                     requireTexture2D,
			const char*                              debugName,
			std::unordered_map<AssetID, CacheEntry>& cacheEntries
		);
		uint64_t CollectGarbageInternal(
			std::unordered_map<AssetID, CacheEntry>& cacheEntries
		);
		uint64_t ReleaseAllInternal(
			std::unordered_map<AssetID, CacheEntry>& cacheEntries
		);

		AssetManager*       mAssetManager         = nullptr;
		RgResourceRegistry* mRegistry             = nullptr;
		uint64_t            mCurrentFrame         = 0;
		uint64_t            mUnusedFrameThreshold = 120;

		std::unordered_map<AssetID, CacheEntry> mSpriteEntries;
		std::unordered_map<AssetID, CacheEntry> mSkyboxEntries;
		std::unordered_map<AssetID, CacheEntry> mMaterial2DEntries;

		TextureResourceCacheDebugStats mDebugStats = {};
	};
}
