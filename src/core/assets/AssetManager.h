#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "AssetID.h"
#include "AssetMetaData.h"
#include "AssetType.h"
#include "LoadResult.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	class ContentPathResolver;
	class IAssetLoader;

	class AssetManager {
	public:
		using ReloadCallback = std::function<void(AssetID id)>;

		struct DebugStats {
			size_t   runtimeAssetCount          = 0;
			size_t   runtimeTextureAssetCount   = 0;
			size_t   destroyedRuntimeAssetCount = 0;
			size_t   loadedTextureAssetCount    = 0;
			uint64_t unloadUnusedFreedCount     = 0;
			uint64_t destroyRuntimeAssetCount   = 0;
		};

		enum class AssetLoadPolicy : uint8_t {
			UseCachedIfLoaded,
			ForceReload,
		};

		/// @brief コンストラクタ
		explicit AssetManager(const ContentPathResolver& contentPathResolver);

		/// @brief アセットローダーを登録します
		/// @param loader 登録するアセットローダー
		void RegisterLoader(std::unique_ptr<IAssetLoader> loader);

		/// @brief ファイルからアセットをロードします。
		/// @deprecated 移行専用APIです。新規コードでは型付きAPI、LoadAsset、またはLoadAssetFromFileを使用してください。
		/// @param path ロードするファイルのパス
		/// @param typeOpt アセットの種類（省略可能）
		/// @param policy ロードポリシー（デフォルトはUseCachedIfLoaded）
		/// @return ロードしたアセットのID
		AssetID LoadFromFile(
			const Path& path,
			std::optional<ASSET_TYPE> typeOpt = std::nullopt,
			AssetLoadPolicy policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief マウント済み content から実行時指定型のアセットをロードします。
		/// @param path 論理アセットパス。
		/// @param type アセットの型。
		/// @param policy ロードポリシー。
		/// @return ロードしたアセットの ID。
		[[nodiscard]] AssetID LoadAsset(
			const VirtualPath& path,
			ASSET_TYPE        type,
			AssetLoadPolicy   policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief 指定mountのcontentから実行時指定型のアセットをロードします。
		/// @param path 論理アセットパス。
		/// @param mountId 解決に使用するmount ID。
		/// @param type アセットの型。
		/// @param policy ロードポリシー。
		/// @return ロードしたアセットの ID。
		[[nodiscard]] AssetID LoadAssetFromMount(
			const VirtualPath& path,
			std::string_view   mountId,
			ASSET_TYPE        type,
			AssetLoadPolicy   policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief 絶対物理ファイルパスから実行時指定型のアセットをロードします。
		/// @param path 解決済みの絶対物理ファイルパス。
		/// @param type アセットの型。
		/// @param policy ロードポリシー。
		/// @return ロードしたアセットの ID。
		[[nodiscard]] AssetID LoadAssetFromFile(
			const Path&     path,
			ASSET_TYPE      type,
			AssetLoadPolicy policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief マウント済み content からテクスチャをロードします。
		/// @param path 論理テクスチャパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたテクスチャアセットの ID。
		[[nodiscard]] AssetID LoadTexture(
			const VirtualPath& path,
			AssetLoadPolicy    policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief 物理ファイルからテクスチャを直接ロードします。
		/// @param path 物理テクスチャファイルパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたテクスチャアセットの ID。
		[[nodiscard]] AssetID LoadTextureFromFile(
			const Path&     path,
			AssetLoadPolicy policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief マウント済み content からメッシュをロードします。
		/// @param path 論理メッシュパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたメッシュアセットの ID。
		AssetID LoadMesh(
			const VirtualPath& path,
			AssetLoadPolicy    policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief 物理ファイルからメッシュを直接ロードします。
		/// @param path 物理メッシュファイルパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたメッシュアセットの ID。
		AssetID LoadMeshFromFile(
			const Path&      path,
			AssetLoadPolicy  policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief マウント済み content からマテリアルインスタンスをロードします。
		/// @param path 論理マテリアルインスタンスパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたマテリアルインスタンスアセットの ID。
		AssetID LoadMaterialInstance(
			const VirtualPath& path,
			AssetLoadPolicy    policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief 物理ファイルからマテリアルインスタンスを直接ロードします。
		/// @param path 物理マテリアルインスタンスファイルパス。
		/// @param policy ロードポリシー。
		/// @return ロードしたマテリアルインスタンスアセットの ID。
		AssetID LoadMaterialInstanceFromFile(
			const Path&      path,
			AssetLoadPolicy  policy = AssetLoadPolicy::UseCachedIfLoaded
		);

		/// @brief ランタイムアセットを作成します
		/// @tparam T ペイロードの型
		/// @param type アセットの種類
		/// @param name アセットの名前
		/// @param payload アセットのペイロードデータ
		/// @param dependencies 依存するアセットのIDリスト
		/// @return 作成したアセットのID
		template <class T>
		AssetID CreateRuntimeAsset(
			ASSET_TYPE                  type,
			std::string                 name,
			T&&                         payload,
			const std::vector<AssetID>& dependencies = {}
		);

		/// @brief ランタイムアセットかどうかを取得します
		/// @param id アセットのID
		/// @return ランタイムアセットの場合true
		[[nodiscard]] bool IsRuntimeAsset(AssetID id) const;

		/// @brief 明示破棄済みのランタイムアセットかどうかを取得します
		/// @param id アセットのID
		/// @return 明示破棄済みの場合true
		[[nodiscard]] bool IsDestroyedRuntimeAsset(AssetID id) const;

		/// @brief 参照されていないランタイムアセットを明示破棄します
		/// @param id 破棄するアセットのID
		/// @return 破棄に成功した場合true
		bool DestroyRuntimeAsset(AssetID id);

		/// @brief アセットの参照カウントを増やします
		/// @param id アセットのID
		void AddRef(AssetID id);

		/// @brief アセットの参照カウントを減らします
		/// @param id アセットのID
		void Release(AssetID id);

		/// @brief アセットの依存関係を設定します
		/// @param id アセットのID
		/// @param dependencies 依存するアセットのIDリスト
		void SetDependencies(
			AssetID                     id,
			const std::vector<AssetID>& dependencies = {}
		);

		/// @brief アセットのメタデータを取得します
		/// @param id アセットのID
		/// @return アセットのメタデータへの参照
		[[nodiscard]] const AssetMetaData& Meta(AssetID id) const;

		/// @brief アセットのペイロードを取得します
		/// @tparam T ペイロードの型
		/// @param id アセットのID
		/// @return アセットのペイロードへのポインタ（型が異なる場合はnullptr）
		template <class T>
		const T* Get(AssetID id) const;

		/// @brief アセットがどのアセットに依存しているかを取得します
		/// @param id アセットのID
		/// @return 依存するアセットのIDリストへの参照
		[[nodiscard]] const std::vector<AssetID>& GetDependencies(
			AssetID id
		) const;

		/// @brief アセットがどのアセットから依存されているかを取得します
		/// @param id アセットのID
		/// @return 被依存するアセットのIDリストへの参照
		[[nodiscard]] const std::vector<AssetID>& GetDependents(
			AssetID id
		) const;

		/// @brief アセットをリロードします
		/// @param id アセットのID
		/// @return リロードに成功したかどうか
		bool                 Reload(AssetID id);
		bool                 ReloadWithDependents(AssetID id);
		std::vector<AssetID> PollSourceChanges();

		/// @brief アセットのリロードコールバックを登録します
		/// @param callback 登録するコールバック関数
		void RegisterReload(ReloadCallback callback);

		/// @brief 参照されていないアセットをアンロードします
		/// @return アンロードしたアセットの数
		[[nodiscard]]
		size_t UnloadUnused();

		/// @brief アセットマネージャーのデバッグ統計を取得します
		/// @return デバッグ統計
		[[nodiscard]] DebugStats GetDebugStats() const;

		/// @brief コンテンツ仮想パス解決に使用する Resolver を取得します。
		[[nodiscard]] const ContentPathResolver& GetContentPathResolver(
		) const noexcept;

		/// @brief パスからアセットを検索します
		/// @param path 検索するアセットのパス
		/// @return 見つかったアセットのID、見つからなかった場合はkInvalidAssetID
		[[nodiscard]] AssetID FindByPath(const Path& path) const;

		/// @brief 名前からアセットを検索します
		/// @param name 検索するアセットの名前
		/// @return 見つかったアセットのID、見つからなかった場合はkInvalidAssetID
		[[nodiscard]] AssetID FindByName(std::string_view name) const;

		std::vector<AssetID> AllAssets() const;

	private:
		/// @brief 新しいアセットIDを割り当てます
		/// @return 割り当てられたアセットID
		AssetID AllocateID();

		/// @brief パスからアセットスロットを検索または作成します
		/// @param path アセットのパス
		/// @param type アセットの型
		/// @return アセットID
		AssetID FindOrCreateSlotByPath(
			const Path& path, ASSET_TYPE type
		);

		/// @brief 解決済みファイルパスからアセットをロードします
		/// @param normalizedPath 正規化されたファイルパス
		/// @param typeOpt アセットの種類
		/// @param policy ロードポリシー
		/// @return ロードしたアセットのID
		AssetID LoadFromResolvedFile(
			const Path&               normalizedPath,
			std::optional<ASSET_TYPE> typeOpt,
			AssetLoadPolicy           policy,
			std::string_view          sourceMountId
		);

		/// @brief 指定したアセットを参照しているアセット情報を再構築します
		/// @param id アセットID
		void RebuildDependents(AssetID id);

		/// @brief すべてのアセットの依存関係情報を再構築します
		void RebuildAllDependents();

		struct Node {
			AssetMetaData        meta;
			AssetPayload         payload;
			std::vector<AssetID> dependencies;
			std::vector<AssetID> dependents;
		};

		mutable std::recursive_mutex mMutex;
		std::vector<Node>            mNodes;
		AssetID                      mNextID = 1;

		std::unordered_map<std::string, AssetID> mPathToID;
		std::unordered_map<std::string, AssetID> mNameToID;

		std::vector<std::unique_ptr<IAssetLoader>> mLoaders;
		std::vector<ReloadCallback>                mReloadCallbacks;
		const ContentPathResolver&                mContentPathResolver;

		uint64_t mUnloadUnusedFreedCount   = 0;
		uint64_t mDestroyRuntimeAssetCount = 0;
	};
}
