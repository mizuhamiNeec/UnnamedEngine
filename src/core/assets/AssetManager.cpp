#include "AssetManager.h"

#include <algorithm>
#include <filesystem>
#include <queue>
#include <unordered_set>

#include <core/UnnamedMacro.h>
#include <core/content/ContentPathResolver.h>
#include <core/filesystem/Path.h>
#include <core/filesystem/VirtualPath.h>

#include <engine/profiler/Profiler.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include "loader/interface/IAssetLoader.h"

#include "types/EventPresentationAssetData.h"
#include "types/MaterialAssetData.h"
#include "types/MaterialInstanceAssetData.h"
#include "types/MeshAssetData.h"
#include "types/PostFxChainAssetData.h"
#include "types/SequenceAssetData.h"
#include "types/ShaderProgramAssetData.h"
#include "types/ShaderSourceAssetData.h"
#include "types/SoundAssetData.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "AstMgr";

	namespace {
		[[nodiscard]] bool IsAbsoluteOrCurrentRelative(const Path& path) {
			if (path.IsEmpty()) {
				return false;
			}

			const std::string text = path.ToGenericUtf8();
			return path.IsAbsolute() || text.starts_with("./") ||
			       text.starts_with("../");
		}

		[[nodiscard]] Path ResolveAssetLoadPath(
			const ContentPathResolver& contentPathResolver,
			const Path&                path
		) {
			const Path normalizedInput = path.LexicallyNormal();
			if (normalizedInput.IsEmpty()) {
				return {};
			}

			if (IsAbsoluteOrCurrentRelative(normalizedInput)) {
				return normalizedInput;
			}

			const std::optional<VirtualPath> virtualPath =
				VirtualPath::Parse(normalizedInput.ToGenericUtf8());
			if (!virtualPath.has_value()) {
				return normalizedInput;
			}

			const std::optional<ResolvedContentFile> resolvedFile =
				contentPathResolver.ResolveFile(*virtualPath);
			if (!resolvedFile.has_value()) {
				return normalizedInput;
			}

			return resolvedFile->resolvedPath;
		}

		FileStamp ReadCurrentFileStamp(const Path& path) {
			FileStamp       stamp = {};
			std::error_code ec;
			if (path.IsEmpty() || !std::filesystem::exists(path.Native(), ec)) {
				return stamp;
			}

			const auto lastWrite = std::filesystem::last_write_time(
				path.Native(), ec
			);
			if (!ec) {
				stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
			}

			stamp.sizeInBytes = std::filesystem::file_size(path.Native(), ec);
			return stamp;
		}

		FileStamp CompleteFileStamp(
			const Path& path, const FileStamp& partialStamp
		) {
			FileStamp completed = partialStamp;
			if (
				completed.sizeInBytes != 0 &&
				completed.lastWriteTicks != 0
			) {
				return completed;
			}

			const FileStamp current = ReadCurrentFileStamp(path);
			if (completed.sizeInBytes == 0) {
				completed.sizeInBytes = current.sizeInBytes;
			}
			if (completed.lastWriteTicks == 0) {
				completed.lastWriteTicks = current.lastWriteTicks;
			}
			return completed;
		}

		bool FileStampEquals(const FileStamp& lhs, const FileStamp& rhs) {
			return lhs.lastWriteTicks == rhs.lastWriteTicks &&
			       lhs.sizeInBytes == rhs.sizeInBytes;
		}
	}

	AssetManager::AssetManager(
		const ContentPathResolver& contentPathResolver
	) : mContentPathResolver(contentPathResolver) {
	}

	void AssetManager::RegisterLoader(std::unique_ptr<IAssetLoader> loader) {
		std::scoped_lock lock(mMutex);
		mLoaders.emplace_back(std::move(loader));
	}

	AssetID AssetManager::LoadFromFile(
		const Path&                     path,
		const std::optional<ASSET_TYPE> typeOpt,
		const AssetLoadPolicy           policy
	) {
		// Transitional compatibility path.
		// Remove after all runtime asset references use VirtualPath explicitly.
		const Path normalizedPath =
			ResolveAssetLoadPath(mContentPathResolver, path);
		if (normalizedPath.IsEmpty()) {
			Warning(kChannel, "Asset path is empty.");
			return kInvalidAssetID;
		}

		return LoadFromResolvedFile(normalizedPath, typeOpt, policy);
	}

	AssetID AssetManager::LoadTexture(
		const VirtualPath&    path,
		const AssetLoadPolicy policy
	) {
		const std::optional<ResolvedContentFile> resolvedFile =
			mContentPathResolver.ResolveFile(path);
		if (!resolvedFile.has_value()) {
			Error(kChannel, "Failed to resolve texture asset: {}",
			      path.String());
			return kInvalidAssetID;
		}

		DevMsg(
			kChannel,
			"Resolved texture asset: virtualPath={}, mount={}, physicalPath={}",
			path.String(),
			resolvedFile->mountId,
			resolvedFile->resolvedPath.ToUtf8()
		);

		return LoadTextureFromFile(resolvedFile->resolvedPath, policy);
	}

	AssetID AssetManager::LoadTextureFromFile(
		const Path&           path,
		const AssetLoadPolicy policy
	) {
		const Path normalizedPath = path.LexicallyNormal();
		if (normalizedPath.IsEmpty()) {
			Warning(kChannel, "Texture path is empty.");
			return kInvalidAssetID;
		}
		if (!normalizedPath.IsRegularFile()) {
			Error(kChannel, "Texture file does not exist: {}", normalizedPath);
			return kInvalidAssetID;
		}

		return LoadFromResolvedFile(
			normalizedPath,
			ASSET_TYPE::TEXTURE,
			policy
		);
	}

	AssetID AssetManager::LoadMesh(
		const VirtualPath&    path,
		const AssetLoadPolicy policy
	) {
		const std::optional<ResolvedContentFile> resolvedFile =
			mContentPathResolver.ResolveFile(path);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Failed to resolve mesh asset: {}",
				path.String()
			);
			return kInvalidAssetID;
		}

		DevMsg(
			kChannel,
			"Resolved mesh asset: virtualPath={}, mount={}, physicalPath={}",
			path.String(),
			resolvedFile->mountId,
			resolvedFile->resolvedPath.ToUtf8()
		);

		return LoadMeshFromFile(resolvedFile->resolvedPath, policy);
	}

	AssetID AssetManager::LoadMeshFromFile(
		const Path&           path,
		const AssetLoadPolicy policy
	) {
		const Path normalizedPath = path.LexicallyNormal();
		if (normalizedPath.IsEmpty()) {
			Warning(kChannel, "Mesh path is empty.");
			return kInvalidAssetID;
		}
		if (!normalizedPath.IsRegularFile()) {
			Error(
				kChannel,
				"Mesh file does not exist: {}",
				normalizedPath.ToUtf8()
			);
			return kInvalidAssetID;
		}

		return LoadFromResolvedFile(
			normalizedPath,
			ASSET_TYPE::MESH,
			policy
		);
	}

	AssetID AssetManager::LoadMaterialInstance(
		const VirtualPath&    path,
		const AssetLoadPolicy policy
	) {
		const std::optional<ResolvedContentFile> resolvedFile =
			mContentPathResolver.ResolveFile(path);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Failed to resolve material instance asset: {}",
				path.String()
			);
			return kInvalidAssetID;
		}

		DevMsg(
			kChannel,
			"Resolved material instance asset: virtualPath={}, mount={}, physicalPath={}",
			path.String(),
			resolvedFile->mountId,
			resolvedFile->resolvedPath.ToUtf8()
		);

		return LoadMaterialInstanceFromFile(resolvedFile->resolvedPath, policy);
	}

	AssetID AssetManager::LoadMaterialInstanceFromFile(
		const Path&           path,
		const AssetLoadPolicy policy
	) {
		const Path normalizedPath = path.LexicallyNormal();
		if (normalizedPath.IsEmpty()) {
			Warning(kChannel, "Material instance path is empty.");
			return kInvalidAssetID;
		}
		if (!normalizedPath.IsRegularFile()) {
			Error(
				kChannel,
				"Material instance file does not exist: {}",
				normalizedPath.ToUtf8()
			);
			return kInvalidAssetID;
		}

		return LoadFromResolvedFile(
			normalizedPath,
			ASSET_TYPE::MATERIAL_INSTANCE,
			policy
		);
	}

	AssetID AssetManager::LoadFromResolvedFile(
		const Path&                     normalizedPath,
		const std::optional<ASSET_TYPE> typeOpt,
		const AssetLoadPolicy           policy
	) {
		Profiler*        profiler = ServiceLocator::Get<Profiler>();
		std::scoped_lock lock(mMutex);

		if (policy == AssetLoadPolicy::UseCachedIfLoaded) {
			const auto cachedIt = mPathToID.
				find(normalizedPath.ToGenericUtf8());
			if (cachedIt != mPathToID.end()) {
				const AssetID cachedId       = cachedIt->second;
				const Node&   cached         = mNodes[cachedId];
				const bool    typeCompatible =
					!typeOpt.has_value() || cached.meta.type == *typeOpt ||
					cached.meta.type == ASSET_TYPE::UNKNOWN;
				if (
					cached.meta.loaded && typeCompatible
				) {
					if (profiler) {
						profiler->AddSample("Asset.Load.CacheHit", 1.0f);
					}
					return cachedId;
				}
				if (cached.meta.loadFailed && typeCompatible) {
					const FileStamp current = ReadCurrentFileStamp(
						cached.meta.sourcePath
					);
					if (FileStampEquals(current, cached.meta.fileStamp)) {
						return kInvalidAssetID;
					}
				}
			}
		}
		auto deduced = ASSET_TYPE::UNKNOWN;
		if (!typeOpt.has_value()) {
			// 型がわかんねぇので、ローダに読めるか確認させる
			for (const auto& l : mLoaders) {
				if (l->CanLoad(normalizedPath, &deduced)) {
					break;
				}
			}

			Warning(
				kChannel,
				"型をチェックしました: {}. 型を知っている場合はなるべく指定してください。",
				ToString(deduced)
			);
		} else {
			deduced = *typeOpt;
		}

		// 不明の場合はスロットだけ作成
		const AssetID id = FindOrCreateSlotByPath(normalizedPath, deduced);
		Node&         n  = mNodes[id];
		if (
			policy == AssetLoadPolicy::UseCachedIfLoaded &&
			n.meta.loaded &&
			(!typeOpt.has_value() || n.meta.type == *typeOpt ||
			 n.meta.type == ASSET_TYPE::UNKNOWN)
		) {
			if (profiler) {
				profiler->AddSample("Asset.Load.CacheHit", 1.0f);
			}
			return id;
		}
		if (
			policy == AssetLoadPolicy::UseCachedIfLoaded &&
			n.meta.loadFailed &&
			(!typeOpt.has_value() || n.meta.type == *typeOpt ||
			 n.meta.type == ASSET_TYPE::UNKNOWN)
		) {
			const FileStamp current = ReadCurrentFileStamp(n.meta.sourcePath);
			if (FileStampEquals(current, n.meta.fileStamp)) {
				return kInvalidAssetID;
			}
		}
		if (profiler) {
			profiler->AddSample("Asset.Load.CacheMiss", 1.0f);
		}

		for (const auto& l : mLoaders) {
			auto t = ASSET_TYPE::UNKNOWN;
			if (!l->CanLoad(normalizedPath, &t)) {
				continue;
			}
			if (typeOpt.has_value() && t != deduced) {
				continue;
			}

			LoadResult r = l->Load(normalizedPath);
			if (std::holds_alternative<std::monostate>(r.payload)) {
				n.payload = std::monostate{};
				n.meta.type = deduced == ASSET_TYPE::UNKNOWN ? t : deduced;
				n.meta.loaded = false;
				n.meta.loadFailed = true;
				n.meta.runtime = false;
				n.meta.destroyed = false;
				n.meta.fileStamp = ReadCurrentFileStamp(n.meta.sourcePath);
				SetDependencies(id, {});
				Error(
					kChannel,
					"Asset loader failed: path={} type={}",
					normalizedPath,
					ToString(n.meta.type)
				);
				return kInvalidAssetID;
			}
			n.payload         = std::move(r.payload);
			n.meta.type       = deduced == ASSET_TYPE::UNKNOWN ? t : deduced;
			n.meta.loaded     = true;
			n.meta.loadFailed = false;
			n.meta.runtime    = false;
			n.meta.destroyed  = false;
			n.meta.fileStamp  = CompleteFileStamp(n.meta.sourcePath, r.stamp);

			// 名前の解決
			if (!r.resolveName.empty()) {
				n.meta.name            = r.resolveName;
				mNameToID[n.meta.name] = id;
			}

			// 依存の設定
			SetDependencies(id, r.dependencies);

			SpecialMsg(
				LogLevel::Success, kChannel,
				"Loaded asset from file: {} (ID: {})", normalizedPath, id
			);

			return id;
		}

		n.meta.loaded     = false;
		n.meta.loadFailed = true;
		n.meta.fileStamp  = ReadCurrentFileStamp(n.meta.sourcePath);
		Error(
			kChannel,
			"No asset loader accepted file: path={} requestedType={}",
			normalizedPath,
			ToString(deduced)
		);
		return kInvalidAssetID;
	}

	template <class T>
	AssetID AssetManager::CreateRuntimeAsset(
		const ASSET_TYPE            type, std::string name, T&& payload,
		const std::vector<AssetID>& dependencies
	) {
		std::scoped_lock lock(mMutex);
		const AssetID    id = AllocateID();
		Node&            n  = mNodes[id];
		n.meta.type         = type;
		n.meta.name         = std::move(name);
		n.meta.sourcePath.Clear();
		n.meta.fileStamp       = {};
		n.meta.runtime         = true;
		n.meta.destroyed       = false;
		n.meta.loaded          = true;
		n.meta.loadFailed      = false;
		n.payload              = std::forward<T>(payload);
		mNameToID[n.meta.name] = id;

		SetDependencies(id, dependencies);
		return id;
	}

	template
	AssetID AssetManager::CreateRuntimeAsset<TextureAssetData>(
		ASSET_TYPE,
		std::string,
		TextureAssetData&&,
		const std::vector<AssetID>&

	
	);

	template
	AssetID AssetManager::CreateRuntimeAsset<SequenceAssetData>(
		ASSET_TYPE,
		std::string,
		SequenceAssetData&&,
		const std::vector<AssetID>&

	
	);

	bool AssetManager::IsRuntimeAsset(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return false;
		}
		return mNodes[id].meta.runtime;
	}

	bool AssetManager::IsDestroyedRuntimeAsset(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return false;
		}
		const AssetMetaData& meta = mNodes[id].meta;
		return meta.runtime && meta.destroyed;
	}

	bool AssetManager::DestroyRuntimeAsset(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			Warning(kChannel,
			        "DestroyRuntimeAsset rejected: invalid assetId={}", id);
			return false;
		}

		Node&          n    = mNodes[id];
		AssetMetaData& meta = n.meta;
		if (!meta.runtime) {
			Warning(
				kChannel,
				"DestroyRuntimeAsset rejected: assetId={} is not a runtime asset.",
				id
			);
			return false;
		}
		if (meta.destroyed) {
			Warning(
				kChannel,
				"DestroyRuntimeAsset rejected: assetId={} is already destroyed.",
				id
			);
			return false;
		}
		if (meta.strongRefs > 0) {
			Warning(
				kChannel,
				"DestroyRuntimeAsset rejected: assetId={} still has strongRefs={}.",
				id,
				meta.strongRefs
			);
			return false;
		}

		std::vector<AssetID> activeDependents;
		for (const AssetID dependent : n.dependents) {
			if (
				dependent == kInvalidAssetID || dependent >= mNextID ||
				mNodes[dependent].meta.destroyed
			) {
				continue;
			}
			activeDependents.emplace_back(dependent);
		}
		if (!activeDependents.empty()) {
			Warning(
				kChannel,
				"DestroyRuntimeAsset rejected: assetId={} is still referenced by {} dependent asset(s).",
				id,
				activeDependents.size()
			);
			return false;
		}

		SetDependencies(id);

		// name lookupは破棄済みruntime assetの誤用検出に使うため、
		// active assetとしては返さずFindByName側でWarningに寄せます。
		const auto pathIt = mPathToID.find(meta.sourcePath.ToGenericUtf8());
		if (
			!meta.sourcePath.IsEmpty() && pathIt != mPathToID.end() &&
			pathIt->second == id
		) {
			mPathToID.erase(pathIt);
		}

		n.payload = std::monostate{};
		n.dependents.clear();
		meta.loaded    = false;
		meta.destroyed = true;
		meta.version++;
		mDestroyRuntimeAssetCount++;

		DevMsg(
			kChannel,
			"Destroyed runtime asset payload: assetId={}, name='{}', type={}",
			id,
			meta.name,
			ToString(meta.type)
		);
		return true;
	}

	void AssetManager::AddRef(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		const AssetMetaData& meta = mNodes[id].meta;
		if (meta.runtime && meta.destroyed) {
			Warning(kChannel, "AddRef ignored for destroyed runtime assetId={}",
			        id);
			return;
		}
		mNodes[id].meta.strongRefs++;
	}

	void AssetManager::Release(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		auto& n = mNodes[id].meta;
		if (n.runtime && n.destroyed) {
			Warning(kChannel,
			        "Release ignored for destroyed runtime assetId={}", id);
			return;
		}
		if (n.strongRefs > 0) {
			n.strongRefs--;
		}
	}

	void AssetManager::SetDependencies(
		AssetID id, const std::vector<AssetID>& dependencies
	) {
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		Node& n = mNodes[id];
		if (n.meta.runtime && n.meta.destroyed) {
			Warning(kChannel,
			        "SetDependencies ignored for destroyed runtime assetId={}",
			        id);
			return;
		}

		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
			auto& vec = mNodes[dep].dependents;
			std::erase(vec, id);
		}

		n.dependencies.clear();
		n.dependencies.reserve(dependencies.size());
		for (const AssetID dep : dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
			if (mNodes[dep].meta.runtime && mNodes[dep].meta.destroyed) {
				Warning(
					kChannel,
					"SetDependencies skipped destroyed runtime dependency: assetId={}, dependency={}",
					id,
					dep
				);
				continue;
			}
			n.dependencies.emplace_back(dep);
		}

		// 依存先のdependentsを更新
		for (const auto dep : n.dependencies) {
			auto& depBy = mNodes[dep].dependents;
			if (std::ranges::find(depBy, id) == depBy.end()) {
				depBy.emplace_back(id);
			}
		}
	}

	const AssetMetaData& AssetManager::Meta(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].meta;
	}

	template <class T>
	const T* AssetManager::Get(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return nullptr;
		}
		const AssetMetaData& meta = mNodes[id].meta;
		if (meta.runtime && meta.destroyed) {
			Warning(kChannel, "Get ignored for destroyed runtime assetId={}",
			        id);
			return nullptr;
		}
		const auto& v = mNodes[id].payload;
		return std::get_if<T>(&const_cast<AssetPayload&>(v));
	}

	template const TextureAssetData*           AssetManager::Get(AssetID) const;
	template const ShaderSourceAssetData*      AssetManager::Get(AssetID) const;
	template const MeshAssetData*              AssetManager::Get(AssetID) const;
	template const ShaderProgramAssetData*     AssetManager::Get(AssetID) const;
	template const MaterialAssetData*          AssetManager::Get(AssetID) const;
	template const MaterialInstanceAssetData*  AssetManager::Get(AssetID) const;
	template const PostFxChainAssetData*       AssetManager::Get(AssetID) const;
	template const SequenceAssetData*          AssetManager::Get(AssetID) const;
	template const SoundAssetData*             AssetManager::Get(AssetID) const;
	template const UiDocumentAssetData*        AssetManager::Get(AssetID) const;
	template const EventPresentationAssetData* AssetManager::Get(AssetID) const;
	template const EditorGuiData*              AssetManager::Get(AssetID) const;

	const std::vector<AssetID>& AssetManager::GetDependencies(
		const AssetID id
	) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependencies;
	}

	const std::vector<AssetID>& AssetManager::GetDependents(
		const AssetID id
	) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependents;
	}

	bool AssetManager::Reload(const AssetID id) {
		std::scoped_lock lock(mMutex);

		// idが無効か?
		if (id == kInvalidAssetID || id >= mNextID) {
			return false;
		}

		Node& n = mNodes[id];
		if (n.meta.runtime && n.meta.destroyed) {
			Warning(kChannel,
			        "Reload rejected for destroyed runtime assetId={}", id);
			return false;
		}

		// ソースパスが空か?
		if (n.meta.sourcePath.IsEmpty()) {
			return false;
		}

		// ローダーを探す
		for (const auto& l : mLoaders) {
			auto t = ASSET_TYPE::UNKNOWN;
			// このローダーで読めるか?
			if (!l->CanLoad(n.meta.sourcePath, &t)) {
				continue;
			}
			// アセットタイプが違うか?
			if (t != n.meta.type && n.meta.type != ASSET_TYPE::UNKNOWN) {
				continue;
			}

			// 再読み込み
			LoadResult r = l->Load(n.meta.sourcePath);
			if (std::holds_alternative<std::monostate>(r.payload)) {
				n.payload         = std::monostate{};
				n.meta.loaded     = false;
				n.meta.loadFailed = true;
				n.meta.fileStamp  = ReadCurrentFileStamp(n.meta.sourcePath);
				SetDependencies(id, {});
				return false;
			}
			n.payload         = std::move(r.payload);
			n.meta.loaded     = true;
			n.meta.loadFailed = false;
			n.meta.destroyed  = false;
			n.meta.fileStamp  = CompleteFileStamp(n.meta.sourcePath, r.stamp);
			n.meta.version++;

			// 依存の設定
			SetDependencies(id, r.dependencies);

			// コールバックの呼び出し
			const auto callbacks = mReloadCallbacks;
			for (auto& cb : callbacks) {
				cb(id);
			}
			return true;
		}

		return false;
	}

	bool AssetManager::ReloadWithDependents(const AssetID id) {
		Profiler*            profiler = ServiceLocator::Get<Profiler>();
		Profiler::ScopeTimer scope(profiler, "Asset.ReloadWithDependents");
		if (!Reload(id)) {
			return false;
		}

		std::unordered_set<AssetID> visited;
		std::queue<AssetID>         queue;
		visited.emplace(id);

		{
			std::scoped_lock lock(mMutex);
			for (const AssetID dependent : mNodes[id].dependents) {
				if (dependent == kInvalidAssetID || dependent >= mNextID) {
					continue;
				}
				if (visited.emplace(dependent).second) {
					queue.push(dependent);
				}
			}
		}

		while (!queue.empty()) {
			const AssetID current = queue.front();
			queue.pop();

			Reload(current);

			std::scoped_lock lock(mMutex);
			for (const AssetID dependent : mNodes[current].dependents) {
				if (dependent == kInvalidAssetID || dependent >= mNextID) {
					continue;
				}
				if (visited.emplace(dependent).second) {
					queue.push(dependent);
				}
			}
		}

		return true;
	}

	std::vector<AssetID> AssetManager::PollSourceChanges() {
		std::vector<AssetID> changed;
		{
			std::scoped_lock lock(mMutex);
			changed.reserve(mNextID);
			for (AssetID id = 1; id < mNextID; ++id) {
				const Node& node = mNodes[id];
				if (
					node.meta.destroyed || !node.meta.loaded ||
					node.meta.sourcePath.IsEmpty()
				) {
					continue;
				}

				const FileStamp current = ReadCurrentFileStamp(
					node.meta.sourcePath
				);
				if (current.sizeInBytes == 0 && current.lastWriteTicks == 0) {
					continue;
				}
				if (!FileStampEquals(current, node.meta.fileStamp)) {
					changed.emplace_back(id);
				}
			}
		}

		std::ranges::sort(changed);
		changed.erase(
			std::ranges::unique(changed).begin(), changed.end()
		);

		std::vector<AssetID> reloaded;
		reloaded.reserve(changed.size());
		for (const AssetID id : changed) {
			if (ReloadWithDependents(id)) {
				reloaded.emplace_back(id);
			}
		}
		return reloaded;
	}

	void AssetManager::RegisterReload(ReloadCallback callback) {
		std::scoped_lock lock(mMutex);
		mReloadCallbacks.emplace_back(std::move(callback));
	}

	size_t AssetManager::UnloadUnused() {
		std::scoped_lock lock(mMutex);
		size_t           freed = 0;
		for (AssetID id = 1; id < mNextID; ++id) {
			Node& n = mNodes[id];
			if (n.meta.runtime && n.meta.destroyed) {
				continue;
			}
			// ロードされていないか?
			if (!n.meta.loaded) {
				continue;
			}
			// 参照されているか?
			if (n.meta.strongRefs > 0) {
				continue;
			}

			// 依存されているか?
			bool needed = false;
			for (const auto depBy : n.dependents) {
				if (depBy < mNextID && mNodes[depBy].meta.strongRefs > 0) {
					needed = true;
					break;
				}
			}

			// 依存されているならスキップ
			if (needed) {
				continue;
			}

			// アンロード
			n.payload     = std::monostate{};
			n.meta.loaded = false;
			freed++;
		}
		mUnloadUnusedFreedCount += freed;
		return freed;
	}

	AssetManager::DebugStats AssetManager::GetDebugStats() const {
		std::scoped_lock lock(mMutex);
		DebugStats       stats         = {};
		stats.unloadUnusedFreedCount   = mUnloadUnusedFreedCount;
		stats.destroyRuntimeAssetCount = mDestroyRuntimeAssetCount;

		for (AssetID id = 1; id < mNextID; ++id) {
			const Node&          node = mNodes[id];
			const AssetMetaData& meta = node.meta;
			if (meta.runtime) {
				stats.runtimeAssetCount++;
				if (meta.type == ASSET_TYPE::TEXTURE) {
					stats.runtimeTextureAssetCount++;
				}
			}
			if (meta.runtime && meta.destroyed) {
				stats.destroyedRuntimeAssetCount++;
			}
			if (
				meta.loaded &&
				std::holds_alternative<TextureAssetData>(node.payload)
			) {
				stats.loadedTextureAssetCount++;
			}
		}

		return stats;
	}

	const ContentPathResolver& AssetManager::GetContentPathResolver(
	) const noexcept {
		return mContentPathResolver;
	}

	AssetID AssetManager::FindByPath(const Path& path) const {
		std::scoped_lock  lock(mMutex);
		const std::string normalized =
			path.LexicallyNormal().ToGenericUtf8();
		const auto it = mPathToID.find(normalized);
		return it != mPathToID.end() ? it->second : kInvalidAssetID;
	}

	AssetID AssetManager::FindByName(const std::string_view name) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mNameToID.find(std::string(name));
		if (it == mNameToID.end()) {
			return kInvalidAssetID;
		}
		const AssetID id = it->second;
		if (id != kInvalidAssetID && id < mNextID) {
			const AssetMetaData& meta = mNodes[id].meta;
			if (meta.runtime && meta.destroyed) {
				Warning(
					kChannel,
					"FindByName ignored destroyed runtime asset: name='{}', assetId={}",
					name,
					id
				);
				return kInvalidAssetID;
			}
		}
		return id;
	}

	std::vector<AssetID> AssetManager::AllAssets() const {
		std::scoped_lock     lock(mMutex);
		std::vector<AssetID> ids;
		ids.reserve(mNextID - 1);
		for (AssetID id = 1; id < mNextID; ++id) {
			ids.emplace_back(id);
		}
		return ids;
	}

	AssetID AssetManager::AllocateID() {
		// ノードのサイズが足りない場合は拡張
		if (mNextID >= mNodes.size()) {
			mNodes.resize(mNextID + 64);
		}
		return mNextID++;
	}

	AssetID AssetManager::FindOrCreateSlotByPath(
		const Path& path, const ASSET_TYPE type
	) {
		const Path normalized = path.LexicallyNormal();
		const auto key        = normalized.ToGenericUtf8();
		const auto it         = mPathToID.find(key);
		if (it != mPathToID.end()) {
			return it->second;
		}

		const AssetID id = AllocateID();
		mPathToID[key]   = id;

		Node& node           = mNodes[id];
		node.meta.type       = type;
		node.meta.sourcePath = normalized;
		node.meta.loaded     = false;
		node.meta.loadFailed = false;
		node.meta.runtime    = false;
		node.meta.destroyed  = false;
		node.meta.name       = Path::ToUtf8String(normalized.FileName());

		mNameToID[node.meta.name] = id;
		return id;
	}

	void AssetManager::RebuildDependents(AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}

		// とりあえず全ノードからidを取り除く
		for (AssetID i = 1; i < mNextID; ++i) {
			auto& depBy = mNodes[i].dependents;
			if (!depBy.empty()) {
				std::erase(depBy, id);
			}
		}

		// idの依存を見て各depの依存に追加
		for (const AssetID d : mNodes[id].dependencies) {
			if (d == kInvalidAssetID || d >= mNextID) {
				continue;
			}
			auto& depBy = mNodes[d].dependents;
			if (std::ranges::find(depBy, id) == depBy.end()) {
				depBy.emplace_back(id);
			}
		}
	}

	void AssetManager::RebuildAllDependents() {
		std::scoped_lock lock(mMutex);
		for (AssetID i = 1; i < mNextID; ++i) {
			mNodes[i].dependents.clear();
		}

		for (AssetID i = 1; i < mNextID; ++i) {
			for (const AssetID d : mNodes[i].dependencies) {
				if (d == kInvalidAssetID || d >= mNextID) {
					continue;
				}
				mNodes[d].dependents.emplace_back(i);
			}
		}
	}
}
