#include "TextureResourceCache.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/TextureAssetData.h"

#include "rendergraph/RgResourceRegistry.h"

namespace Unnamed::Render {
	void TextureResourceCache::Initialize(
		AssetManager* const assetManager, RgResourceRegistry* const registry
	) {
		mAssetManager = assetManager;
		mRegistry     = registry;
	}

	void TextureResourceCache::BeginFrame(const uint64_t frameIndex) {
		mCurrentFrame                      = frameIndex;
		mDebugStats.lastFrameReleasedByTtl = 0;
	}

	void TextureResourceCache::SetUnusedFrameThreshold(
		const uint64_t thresholdFrames
	) {
		mUnusedFrameThreshold = thresholdFrames;
	}

	uint32_t TextureResourceCache::ResolveSpriteTexture(const AssetID assetId) {
		return ResolveTexture(
			assetId, false, false, "SpriteOverlayTex", mSpriteEntries
		);
	}

	uint32_t TextureResourceCache::ResolveSkyboxTexture(const AssetID assetId) {
		return ResolveTexture(
			assetId, true, false, "SkyboxCubeTex", mSkyboxEntries
		);
	}

	uint32_t TextureResourceCache::ResolveTexture2D(const AssetID assetId) {
		return ResolveTexture(
			assetId, false, true, "MaterialTex2D", mMaterial2DEntries
		);
	}

	void TextureResourceCache::CollectGarbage() {
		const uint64_t releasedSprite = CollectGarbageInternal(mSpriteEntries);
		const uint64_t releasedSkybox = CollectGarbageInternal(mSkyboxEntries);
		// Material bindings keep resolved texture IDs until they are rebuilt, so
		// material entries are released by ReleaseAll instead of TTL collection.
		mDebugStats.lastFrameReleasedByTtl = releasedSprite + releasedSkybox;
	}

	void TextureResourceCache::ReleaseAll() {
		const uint64_t releasedSprite   = ReleaseAllInternal(mSpriteEntries);
		const uint64_t releasedSkybox   = ReleaseAllInternal(mSkyboxEntries);
		const uint64_t releasedMaterial =
			ReleaseAllInternal(mMaterial2DEntries);
		mDebugStats.releaseAllReleaseCount +=
			releasedSprite + releasedSkybox + releasedMaterial;
	}

	TextureResourceCacheDebugStats TextureResourceCache::GetDebugStats() const {
		TextureResourceCacheDebugStats stats = mDebugStats;
		stats.spriteEntryCount = static_cast<uint32_t>(mSpriteEntries.size());
		stats.skyboxEntryCount = static_cast<uint32_t>(mSkyboxEntries.size());
		stats.materialEntryCount =
			static_cast<uint32_t>(mMaterial2DEntries.size());
		stats.liveEntryCount = stats.spriteEntryCount +
		                       stats.skyboxEntryCount +
		                       stats.materialEntryCount;
		return stats;
	}

	uint32_t TextureResourceCache::ResolveTexture(
		const AssetID                            assetId,
		const bool                               requireCubeMap,
		const bool                               requireTexture2D,
		const char* const                        debugName,
		std::unordered_map<AssetID, CacheEntry>& cacheEntries
	) {
		if (
			assetId == kInvalidAssetID ||
			mAssetManager == nullptr ||
			mRegistry == nullptr
		) {
			mDebugStats.failedResolveCount++;
			return 0;
		}

		const AssetMetaData& meta = mAssetManager->Meta(assetId);
		if (meta.runtime && meta.destroyed) {
			if (const auto it = cacheEntries.find(assetId);
				it != cacheEntries.end()) {
				if (it->second.textureId != 0) {
					mRegistry->ReleaseTexture(it->second.textureId);
				}
				cacheEntries.erase(it);
			}
			mDebugStats.failedResolveCount++;
			return 0;
		}

		const TextureAssetData* texture = mAssetManager->Get<TextureAssetData>(
			assetId
		);
		if (!texture) {
			mDebugStats.failedResolveCount++;
			return 0;
		}
		if (requireCubeMap && !texture->isCubeMap) {
			mDebugStats.failedResolveCount++;
			return 0;
		}
		if (requireTexture2D && texture->isCubeMap) {
			mDebugStats.failedResolveCount++;
			return 0;
		}

		const uint32_t assetVersion = meta.version;
		if (const auto it = cacheEntries.find(assetId);
			it != cacheEntries.end()) {
			CacheEntry& entry = it->second;
			if (
				entry.textureId != 0 &&
				entry.assetVersion == assetVersion
			) {
				entry.lastUsedFrame = mCurrentFrame;
				return entry.textureId;
			}

			if (entry.textureId != 0) {
				mRegistry->ReleaseTexture(entry.textureId);
				mDebugStats.versionRecreateCount++;
			}
			cacheEntries.erase(it);
		}

		const uint32_t textureId = requireCubeMap ?
			                           mRegistry->CreateTextureFromAsset(
				                           *texture, debugName
			                           ) :
			                           mRegistry->CreateTexture2DFromAsset(
				                           *texture, debugName
			                           );
		if (textureId == 0) {
			mDebugStats.failedResolveCount++;
			return 0;
		}

		cacheEntries.emplace(
			assetId,
			CacheEntry{
				.textureId     = textureId,
				.assetVersion  = assetVersion,
				.lastUsedFrame = mCurrentFrame,
			}
		);
		mDebugStats.createdTextureCount++;
		return textureId;
	}

	uint64_t TextureResourceCache::CollectGarbageInternal(
		std::unordered_map<AssetID, CacheEntry>& cacheEntries
	) {
		if (mRegistry == nullptr) {
			return 0;
		}

		uint64_t releasedCount = 0;
		for (auto it = cacheEntries.begin(); it != cacheEntries.end();) {
			const CacheEntry& entry         = it->second;
			const bool        shouldRelease =
				mCurrentFrame > entry.lastUsedFrame &&
				mCurrentFrame - entry.lastUsedFrame > mUnusedFrameThreshold;
			if (!shouldRelease) {
				++it;
				continue;
			}

			if (entry.textureId != 0) {
				mRegistry->ReleaseTexture(entry.textureId);
				++releasedCount;
			}
			it = cacheEntries.erase(it);
		}

		mDebugStats.ttlReleaseCount += releasedCount;
		return releasedCount;
	}

	uint64_t TextureResourceCache::ReleaseAllInternal(
		std::unordered_map<AssetID, CacheEntry>& cacheEntries
	) {
		if (mRegistry == nullptr) {
			cacheEntries.clear();
			return 0;
		}

		uint64_t releasedCount = 0;
		for (const auto& [assetId, entry] : cacheEntries) {
			(void)assetId;
			if (entry.textureId == 0) {
				continue;
			}
			mRegistry->ReleaseTexture(entry.textureId);
			++releasedCount;
		}
		cacheEntries.clear();
		return releasedCount;
	}
}
