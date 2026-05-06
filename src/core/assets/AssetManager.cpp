#include "AssetManager.h"

#include <algorithm>
#include <filesystem>
#include <queue>
#include <unordered_set>

#include <core/UnnamedMacro.h>
#include <core/string/StrUtil.h>

#include <engine/game/GamePathResolver.h>
#include <engine/game/IGameModule.h>
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
		[[nodiscard]] bool IsCurrentDirectoryRelativePath(
			const std::string_view path
		) {
			return path.starts_with("./") || path.starts_with("../");
		}

		[[nodiscard]] bool IsEngineRootRelativePath(
			const std::string_view path
		) {
			return path.starts_with("content/") ||
			       path.starts_with("projects/");
		}

		[[nodiscard]] std::string ResolveAssetLoadPath(
			const std::string_view path
		) {
			const std::string normalizedInput = StrUtil::NormalizePath(
				std::string(path)
			);
			if (normalizedInput.empty()) {
				return {};
			}

			const std::filesystem::path fsPath(normalizedInput);
			if (
				fsPath.is_absolute() ||
				IsCurrentDirectoryRelativePath(normalizedInput)
			) {
				return normalizedInput;
			}

			// 既存データの "content/..." や "projects/..." はプロジェクトルート基準で扱います。
			if (IsEngineRootRelativePath(normalizedInput)) {
				return "./" + normalizedInput;
			}

			const IGameModule* gameModule = ServiceLocator::Get<IGameModule>();
			if (!gameModule) {
				return normalizedInput;
			}

			const GameModulePaths gamePaths = gameModule->GetGameModulePaths();
			const MountedContentResolution resolution =
				ResolveGameMountedContentPathDetailed(gamePaths, normalizedInput);
			if (!resolution.resolvedRoot.empty()) {
				DevMsg(
					kChannel,
					"asset mount resolved '{}' -> '{}' layer='{}' root='{}' exists={}",
					normalizedInput,
					resolution.resolvedPath,
					resolution.resolvedLayer,
					resolution.resolvedRoot,
					resolution.existsOnDisk
				);
			}
			return resolution.resolvedPath.empty() ?
				       normalizedInput :
				       resolution.resolvedPath;
		}

		FileStamp ReadCurrentFileStamp(const std::string& path) {
			FileStamp       stamp = {};
			std::error_code ec;
			if (!std::filesystem::exists(path, ec)) {
				return stamp;
			}

			const auto lastWrite = std::filesystem::last_write_time(path, ec);
			if (!ec) {
				stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
			}

			stamp.sizeInBytes = std::filesystem::file_size(path, ec);
			return stamp;
		}

		FileStamp CompleteFileStamp(
			const std::string& path, const FileStamp& partialStamp
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

	AssetManager::AssetManager() = default;

	void AssetManager::RegisterLoader(std::unique_ptr<IAssetLoader> loader) {
		std::scoped_lock lock(mMutex);
		mLoaders.emplace_back(std::move(loader));
	}

	AssetID AssetManager::LoadFromFile(
		const std::string&              path,
		const std::optional<ASSET_TYPE> typeOpt,
		const AssetLoadPolicy           policy
	) {
		const std::string normalizedPath = StrUtil::NormalizePath(
			ResolveAssetLoadPath(path)
		);
		if (normalizedPath.empty()) {
			Warning(kChannel, "Asset path is empty.");
			return kInvalidAssetID;
		}

		Profiler*        profiler = ServiceLocator::Get<Profiler>();
		std::scoped_lock lock(mMutex);

		if (policy == AssetLoadPolicy::UseCachedIfLoaded) {
			const auto cachedIt = mPathToID.find(normalizedPath);
			if (cachedIt != mPathToID.end()) {
				const AssetID cachedId = cachedIt->second;
				const Node&   cached   = mNodes[cachedId];
				if (
					cached.meta.loaded &&
					(!typeOpt.has_value() || cached.meta.type == *typeOpt ||
					 cached.meta.type == ASSET_TYPE::UNKNOWN)
				) {
					if (profiler) {
						profiler->AddSample("Asset.Load.CacheHit", 1.0f);
					}
					return cachedId;
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

			LoadResult r     = l->Load(normalizedPath);
			n.payload        = std::move(r.payload);
			n.meta.type      = deduced == ASSET_TYPE::UNKNOWN ? t : deduced;
			n.meta.loaded    = true;
			n.meta.fileStamp = CompleteFileStamp(n.meta.sourcePath, r.stamp);

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

		n.meta.loaded = false;
		return id;
	}

	template <class T>
	AssetID AssetManager::CreateRuntimeAsset(
		const ASSET_TYPE            type, std::string name, T&& payload,
		const std::vector<AssetID>& dependencies
	) {
		std::scoped_lock lock(mMutex);
		const AssetID    id    = AllocateID();
		Node&            n     = mNodes[id];
		n.meta.type            = type;
		n.meta.name            = std::move(name);
		n.meta.loaded          = true;
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

	void AssetManager::AddRef(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
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

		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
			auto& vec = mNodes[dep].dependents;
			std::erase(vec, id);
		}

		n.dependencies = dependencies;

		// 依存先のdependentsを更新
		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
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

		// ソースパスが空か?
		if (n.meta.sourcePath.empty()) {
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
			LoadResult r     = l->Load(n.meta.sourcePath);
			n.payload        = std::move(r.payload);
			n.meta.loaded    = true;
			n.meta.fileStamp = CompleteFileStamp(n.meta.sourcePath, r.stamp);
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
				if (!node.meta.loaded || node.meta.sourcePath.empty()) {
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
		return freed;
	}

	AssetID AssetManager::FindByPath(const std::string_view path) const {
		std::scoped_lock  lock(mMutex);
		const std::string normalized =
			StrUtil::NormalizePath(std::string(path));
		const auto it = mPathToID.find(normalized);
		return it != mPathToID.end() ? it->second : kInvalidAssetID;
	}

	AssetID AssetManager::FindByName(const std::string_view name) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mNameToID.find(std::string(name));
		return it != mNameToID.end() ? it->second : kInvalidAssetID;
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
		const std::string& path, const ASSET_TYPE type
	) {
		const auto normalized = StrUtil::NormalizePath(path);
		const auto it         = mPathToID.find(normalized);
		if (it != mPathToID.end()) {
			return it->second;
		}

		const AssetID id      = AllocateID();
		mPathToID[normalized] = id;

		namespace Fs = std::filesystem;

		Node& node           = mNodes[id];
		node.meta.type       = type;
		node.meta.sourcePath = normalized;
		node.meta.loaded     = false;
		node.meta.name       = Fs::path(normalized).filename().string();

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
