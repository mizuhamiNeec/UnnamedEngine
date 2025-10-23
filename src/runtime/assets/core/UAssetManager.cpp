#include "UAssetManager.h"

#include <pch.h>

#include <filesystem>

#include "UAsset.h"

#include "runtime/assets/loaders/interface/IAssetLoader.h"

namespace fs = std::filesystem;

namespace Unnamed {
	constexpr std::string_view kChannel = "UAssetManager";

	/// @brief UASSET_TYPEを文字列に変換します
	/// @param e 変換するUASSET_TYPE
	/// @return 変換後の文字列
	const char* ToString(const UASSET_TYPE e) {
		switch (e) {
		case UASSET_TYPE::UNKNOWN: return "UNKNOWN";
		case UASSET_TYPE::TEXTURE: return "TEXTURE";
		case UASSET_TYPE::SHADER: return "SHADER";
		case UASSET_TYPE::MATERIAL: return "MATERIAL";
		case UASSET_TYPE::MESH: return "MESH";
		case UASSET_TYPE::SOUND: return "SOUND";
		default: return "unknown";
		}
	}

	/// @brief コンストラクタ
	UAssetManager::UAssetManager() = default;

	/// @brief アセットローダーを登録します
	/// @param loader 登録するローダー
	void UAssetManager::RegisterLoader(std::unique_ptr<IAssetLoader> loader) {
		std::scoped_lock lock(mMutex);
		mLoaders.emplace_back(std::move(loader));
	}

	/// @brief ファイルからアセットを読み込みます
	/// @param path アセットのファイルパス
	/// @param typeOpt 読み込むアセットの型
	AssetID UAssetManager::LoadFromFile(
		const std::string&               path,
		const std::optional<UASSET_TYPE> typeOpt
	) {
		std::scoped_lock lock(mMutex);
		auto             deduced = UASSET_TYPE::UNKNOWN;
		if (!typeOpt.has_value()) {
			// 型がわかんねぇので、ローダーに読めるか確認させる
			for (const auto& l : mLoaders) {
				if (l->CanLoad(path, &deduced)) {
					break;
				}
			}

			Warning(
				kChannel,
				"型をチェックしました: {}",
				ToString(deduced)
			);
		} else {
			deduced = *typeOpt;
		}

		// 不明の場合はスロットだけ作成
		const AssetID id = FindOrCreateSlotByPath(path, deduced);
		Node&         n  = mNodes[id];

		for (const auto& l : mLoaders) {
			auto t = UASSET_TYPE::UNKNOWN;
			if (!l->CanLoad(path, &t)) {
				continue;
			}
			if (typeOpt.has_value() && t != deduced) {
				continue;
			}

			LoadResult r     = l->Load(path);
			n.payload        = std::move(r.payload);
			n.meta.type      = (deduced == UASSET_TYPE::UNKNOWN) ? t : deduced;
			n.meta.loaded    = true;
			n.meta.fileStamp = r.stamp;
			if (!r.resolveName.empty()) {
				n.meta.name            = r.resolveName;
				mNameToID[n.meta.name] = id;
			}

			// 依存の設定
			SetDependencies(id, r.dependencies);
			return id;
		}

		n.meta.loaded = false;
		return id;
	}

	/// @brief ランタイムアセットを作成します
	/// @param type アセットの型
	/// @param name アセットの名前
	/// @param payload アセットのペイロード
	/// @param dependencies アセットの依存関係
	template <class T>
	AssetID UAssetManager::CreateRuntimeAsset(
		const UASSET_TYPE           type,
		std::string                 name,
		T&&                         payload,
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
	
	template AssetID UAssetManager::CreateRuntimeAsset<TextureAssetData>(
		UASSET_TYPE, std::string,
		TextureAssetData&&, const std::vector<AssetID>&
	);
	
	template AssetID UAssetManager::CreateRuntimeAsset<ShaderAssetData>(
		UASSET_TYPE, std::string,
		ShaderAssetData&&, const std::vector<AssetID>&
	);
	
	template AssetID UAssetManager::CreateRuntimeAsset<MaterialAssetData>(
		UASSET_TYPE, std::string,
		MaterialAssetData&&, const std::vector<AssetID>&
	);
	
	template AssetID UAssetManager::CreateRuntimeAsset<MeshAssetData>(
		UASSET_TYPE, std::string,
		MeshAssetData&&, const std::vector<AssetID>&
	);

	template AssetID UAssetManager::CreateRuntimeAsset<SoundAssetData>(
		UASSET_TYPE, std::string,
		SoundAssetData&&, const std::vector<AssetID>&
	);

	/// @brief アセットの参照カウントを増加させます
	/// @param id アセットID
	void UAssetManager::AddRef(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		mNodes[id].meta.strongRefs++;
	}

	/// @brief アセットの参照カウントを減少させます
	/// @param id アセットID
	void UAssetManager::Release(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		auto& n = mNodes[id].meta;
		if (n.strongRefs > 0) {
			n.strongRefs--;
		}
	}

	/// @brief アセットの依存関係を設定します
	/// @param id アセットID
	/// @param dependencies 依存関係のアセットIDリスト
	void UAssetManager::SetDependencies(
		AssetID                     id,
		const std::vector<AssetID>& dependencies
	) {
		if (id == kInvalidAssetID || id >= mNextID) {
			return;
		}
		Node& n = mNodes[id];

		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
			auto& vec = mNodes[dep].dependencies;
			std::erase(vec, id);
		}

		n.dependencies = dependencies;

		// 新しく逆辺を張る
		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) {
				continue;
			}
			mNodes[dep].dependents.emplace_back(id);
		}
	}

	/// @brief アセットのメタデータを取得します
	/// @param id アセットID
	/// @return アセットのメタデータへの参照
	const AssetMetaData& UAssetManager::Meta(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].meta;
	}

	/// @brief アセットのペイロードを取得します
	/// @param id アセットID
	/// @return アセットのペイロードへのポインタ（型が異なる場合はnullptr）
	template <class T>
	const T* UAssetManager::Get(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return nullptr;
		}
		const auto& v = mNodes[id].payload;
		return std::get_if<T>(&const_cast<AssetPayload&>(v));
	}

	template const TextureAssetData* UAssetManager::Get<TextureAssetData>(
		AssetID id
	) const;
	template const ShaderAssetData* UAssetManager::Get<ShaderAssetData>(
		AssetID id
	) const;
	template const MaterialAssetData* UAssetManager::Get<MaterialAssetData>(
		AssetID id
	) const;
	template const MeshAssetData* UAssetManager::Get<MeshAssetData>(
		AssetID id
	) const;
	template const SoundAssetData* UAssetManager::Get<SoundAssetData>(
		AssetID id
	) const;

	/// @brief アセットの依存関係を取得します
	/// @param id アセットID
	/// @return 依存関係のアセットIDリストへの参照
	const std::vector<AssetID>& UAssetManager::Dependencies(
		const AssetID id) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependencies;
	}

	/// @brief アセットを参照しているアセットのリストを取得します
	/// @param id アセットID
	/// @return 参照しているアセットIDリストへの参照
	const std::vector<AssetID>& UAssetManager::Dependents(
		const AssetID id) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependents;
	}

	/// @brief アセットを再読み込みします
	/// @param id アセットID
	/// @return 成功したらtrueを返す
	bool UAssetManager::Reload(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) {
			return false;
		}
		Node& n = mNodes[id];
		if (n.meta.sourcePath.empty()) {
			return false;
		}

		for (const auto& l : mLoaders) {
			auto t = UASSET_TYPE::UNKNOWN;
			if (!l->CanLoad(n.meta.sourcePath, &t)) {
				continue;
			}
			if (t != n.meta.type && n.meta.type != UASSET_TYPE::UNKNOWN) {
				continue;
			}

			LoadResult r     = l->Load(n.meta.sourcePath);
			n.payload        = std::move(r.payload);
			n.meta.loaded    = true;
			n.meta.fileStamp = r.stamp;
			n.meta.version++;

			SetDependencies(id, r.dependencies);

			auto callbacks = mReloadCallbacks;

			for (auto& cb : callbacks) {
				cb(id);
			}
			return true;
		}

		return false;
	}

	/// @brief アセット再読み込みのコールバックを登録します
	/// @param callback コールバック関数
	void UAssetManager::RegisterReload(ReloadCallback callback) {
		std::scoped_lock lock(mMutex);
		mReloadCallbacks.emplace_back(std::move(callback));
	}

	/// @brief 参照されていないアセットをアンロードします
	/// @return アンロードしたアセットの数
	size_t UAssetManager::UnloadUnused() {
		std::scoped_lock lock(mMutex);
		size_t           freed = 0;
		for (AssetID id = 1; id < mNextID; ++id) {
			Node& n = mNodes[id];
			if (!n.meta.loaded) {
				continue;
			}
			if (n.meta.strongRefs > 0) {
				continue;
			}

			bool needed = false;
			for (auto depBy : n.dependents) {
				if (depBy < mNextID && mNodes[depBy].meta.strongRefs > 0) {
					needed = true;
					break;
				}
			}

			if (needed) {
				continue;
			}

			n.payload     = std::monostate{};
			n.meta.loaded = false;
			freed++;
		}
		return freed;
	}

	/// @brief パスからアセットIDを検索します
	/// @param path アセットのパス
	/// @return アセットID（見つからなかった場合はkInvalidAssetID）
	AssetID UAssetManager::FindByPath(const std::string_view path) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mPathToID.find(std::string(path));
		return it != mPathToID.end() ? it->second : kInvalidAssetID;
	}

	/// @brief 名前からアセットIDを検索します
	/// @param name アセットの名前
	/// @return アセットID（見つからなかった場合はkInvalidAssetID）
	AssetID UAssetManager::FindByName(const std::string_view name) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mNameToID.find(std::string(name));
		return it != mNameToID.end() ? it->second : kInvalidAssetID;
	}

	/// @brief すべてのアセットIDを取得します
	/// @return アセットIDのリスト
	std::vector<AssetID> UAssetManager::AllAssets() const {
		std::scoped_lock     lock(mMutex);
		std::vector<AssetID> ids;
		ids.reserve(mNextID - 1);
		for (AssetID id = 1; id < mNextID; ++id) {
			ids.emplace_back(id);
		}
		return ids;
	}

	/// @brief 新しいアセットIDを割り当てます
	/// @return 割り当てられたアセットID
	AssetID UAssetManager::AllocateID() {
		// 0は無効
		if (mNextID >= mNodes.size()) {
			mNodes.resize(static_cast<size_t>(mNextID) + 64);
		}
		return mNextID++;
	}

	/// @brief パスからアセットスロットを検索または作成します
	/// @param path アセットのパス
	/// @param type アセットの型
	/// @return アセットID
	AssetID UAssetManager::FindOrCreateSlotByPath(
		const std::string& path,
		const UASSET_TYPE  type
	) {
		const auto normalized = NormalizePath(path);
		const auto it         = mPathToID.find(normalized);
		if (it != mPathToID.end()) {
			return it->second;
		}

		const AssetID id      = AllocateID();
		mPathToID[normalized] = id;

		Node& node           = mNodes[id];
		node.meta.type       = type;
		node.meta.sourcePath = normalized;
		node.meta.loaded     = false;
		node.meta.name       = fs::path(normalized).filename().string();

		mNameToID[node.meta.name] = id;
		return id;
	}

	/// @brief 指定したアセットを参照しているアセット情報を再構築します
	/// @param id アセットID
	void UAssetManager::RebuildDependents(AssetID id) {
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
		// idの依存を見て各depの依存に追加する
		for (const AssetID d : mNodes[id].dependencies) {
			if (d == kInvalidAssetID || d >= mNextID) {
				auto& depBy = mNodes[d].dependents;
				if (std::ranges::find(depBy, id) == depBy.end()) {
					depBy.emplace_back(id);
				}
			}
		}
	}

	/// @brief すべてのアセットの参照情報を再構築します
	void UAssetManager::RebuildAllDependents() {
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

	/// @brief パスを正規化します（バックスラッシュをスラッシュに変換）
	/// @param path 正規化するパス
	/// @return 正規化後のパス
	std::string UAssetManager::NormalizePath(std::string path) {
		for (auto& c : path) {
			if (c == '\\') {
				c = '/';
			}
		}
		return path;
	}
}
