#include "UiDocumentManager.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/UiDocumentAssetData.h"
#include "core/io/json/JsonReader.h"

#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::Gui {
	static constexpr std::string_view kChannel = "UiDocument";

	UiDocumentManager::UiDocumentManager(AssetManager* assetManager) :
		mAssetManager(assetManager) {
		if (!mAssetManager) {
			mAssetManager = ServiceLocator::Get<AssetManager>();
		}
	}

	UiDocumentManager::~UiDocumentManager() = default;

	std::shared_ptr<UiDocument> UiDocumentManager::LoadDocument(
		const Path& path
	) {
		if (!mAssetManager) {
			Error(kChannel, "AssetManager is not available.");
			return nullptr;
		}

		const Path        normalizedPath = NormalizePath(path);
		const std::string key            = normalizedPath.ToGenericUtf8();
		ManagedDocument&  managed        = mDocuments[key];
		managed.normalizedPath = normalizedPath;
		managed.assetId = mAssetManager->LoadFromFile(
			normalizedPath, ASSET_TYPE::UI_DOCUMENT
		);
		if (managed.assetId == kInvalidAssetID) {
			Error(kChannel, "Failed to load UI document asset '{}'.", normalizedPath);
			return nullptr;
		}

		if (!ReloadDocumentFromAsset(managed)) {
			Error(kChannel, "Failed to decode UI document '{}'.", normalizedPath);
			return nullptr;
		}

		managed.dirty           = false;
		managed.pendingExternal = false;
		DevMsg(kChannel, "Loaded UiDocument asset: {}", normalizedPath);
		return managed.document;
	}

	void UiDocumentManager::UnloadDocument(const Path& path) {
		mDocuments.erase(NormalizePath(path).ToGenericUtf8());
	}

	std::shared_ptr<UiDocument> UiDocumentManager::GetDocument(
		const Path& path
	) const {
		const ManagedDocument* managed = FindManaged(path);
		return managed ? managed->document : nullptr;
	}

	bool UiDocumentManager::SaveDocument(
		const Path&                      path,
		const std::shared_ptr<UiDocument>& document
	) {
		if (!document) {
			return false;
		}

		const Path        normalizedPath = NormalizePath(path);
		const std::string key            = normalizedPath.ToGenericUtf8();
		if (!document->Save(normalizedPath)) {
			return false;
		}

		ManagedDocument& managed = mDocuments[key];
		managed.normalizedPath = normalizedPath;
		managed.document       = document;
		managed.dirty          = false;
		managed.pendingExternal = false;

		if (!mAssetManager) {
			return true;
		}

		managed.assetId = mAssetManager->LoadFromFile(
			normalizedPath,
			ASSET_TYPE::UI_DOCUMENT,
			AssetManager::AssetLoadPolicy::ForceReload
		);
		if (managed.assetId == kInvalidAssetID) {
			return true;
		}

		managed.loadedVersion = mAssetManager->Meta(managed.assetId).version;
		return true;
	}

	void UiDocumentManager::MarkDirty(const Path& path, const bool dirty) {
		if (ManagedDocument* managed = FindManaged(path)) {
			managed->dirty = dirty;
		}
	}

	bool UiDocumentManager::IsDirty(const Path& path) const {
		if (const ManagedDocument* managed = FindManaged(path)) {
			return managed->dirty;
		}
		return false;
	}

	bool UiDocumentManager::HasPendingExternal(const Path& path) const {
		if (const ManagedDocument* managed = FindManaged(path)) {
			return managed->pendingExternal;
		}
		return false;
	}

	void UiDocumentManager::ResolvePendingExternal(
		const Path&        path,
		const bool         reloadFromAsset
	) {
		ManagedDocument* managed = FindManaged(path);
		if (!managed || !managed->pendingExternal) {
			return;
		}

		if (reloadFromAsset) {
			if (ReloadDocumentFromAsset(*managed)) {
				managed->dirty = false;
			}
		}
		managed->pendingExternal = false;
	}

	std::vector<Path> UiDocumentManager::UpdateTrackedDocuments() {
		std::vector<Path> updatedPaths;
		if (!mAssetManager) {
			return updatedPaths;
		}

		for (auto& [path, managed] : mDocuments) {
			if (managed.assetId == kInvalidAssetID) {
				continue;
			}

			const uint64_t currentVersion = mAssetManager->Meta(managed.assetId).
			                                              version;
			if (currentVersion <= managed.loadedVersion) {
				continue;
			}

			if (managed.dirty) {
				managed.pendingExternal = true;
				continue;
			}

			if (ReloadDocumentFromAsset(managed)) {
				managed.pendingExternal = false;
				updatedPaths.emplace_back(managed.normalizedPath);
			}
		}

		return updatedPaths;
	}

	Path UiDocumentManager::NormalizePath(Path path) {
		return path.IsEmpty() ? Path() : path.LexicallyNormal();
	}

	bool UiDocumentManager::ReloadDocumentFromAsset(ManagedDocument& managed) const {
		if (!mAssetManager || managed.assetId == kInvalidAssetID) {
			return false;
		}

		const auto* assetData = mAssetManager->Get<UiDocumentAssetData>(
			managed.assetId
		);
		if (!assetData) {
			return false;
		}

		auto doc = UiDocument::LoadFromJson(
			JsonReader(assetData->rootJson),
			managed.normalizedPath.ToGenericUtf8()
		);
		if (!doc) {
			return false;
		}

		managed.document      = std::move(doc);
		managed.loadedVersion = mAssetManager->Meta(managed.assetId).version;
		return true;
	}

	UiDocumentManager::ManagedDocument* UiDocumentManager::FindManaged(
		const Path& path
	) {
		const std::string normalizedPath =
			NormalizePath(path).ToGenericUtf8();
		const auto it = mDocuments.find(normalizedPath);
		if (it == mDocuments.end()) {
			return nullptr;
		}
		return &it->second;
	}

	const UiDocumentManager::ManagedDocument* UiDocumentManager::FindManaged(
		const Path& path
	) const {
		const std::string normalizedPath =
			NormalizePath(path).ToGenericUtf8();
		const auto it = mDocuments.find(normalizedPath);
		if (it == mDocuments.end()) {
			return nullptr;
		}
		return &it->second;
	}
}
