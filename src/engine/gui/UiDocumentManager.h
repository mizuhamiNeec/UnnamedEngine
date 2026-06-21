#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"

#include "UiDocument.h"

namespace Unnamed::Gui {
	class UiDocument;
}

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Gui {
	class UiDocumentManager {
	public:
		explicit UiDocumentManager(AssetManager* assetManager = nullptr);
		~UiDocumentManager();

		std::shared_ptr<UiDocument> LoadDocument(const Path& path);
		void                        UnloadDocument(const Path& path);

		std::shared_ptr<UiDocument> GetDocument(const Path& path) const;
		bool                        SaveDocument(
			const Path&                        path,
			const std::shared_ptr<UiDocument>& document
		);

		void MarkDirty(const Path& path, bool dirty = true);
		[[nodiscard]] bool IsDirty(const Path& path) const;
		[[nodiscard]] bool HasPendingExternal(const Path& path) const;
		void ResolvePendingExternal(const Path& path, bool reloadFromAsset);
		std::vector<Path> UpdateTrackedDocuments();

	private:
		struct ManagedDocument {
			Path                        normalizedPath;
			AssetID                     assetId       = kInvalidAssetID;
			uint64_t                    loadedVersion = 0;
			std::shared_ptr<UiDocument> document;
			bool                        dirty           = false;
			bool                        pendingExternal = false;
		};

		static Path NormalizePath(const Path& path);
		bool ReloadDocumentFromAsset(ManagedDocument& managed) const;
		ManagedDocument* FindManaged(const Path& path);
		const ManagedDocument* FindManaged(const Path& path) const;

		AssetManager* mAssetManager = nullptr;
		std::unordered_map<std::string, ManagedDocument> mDocuments;
	};
}
