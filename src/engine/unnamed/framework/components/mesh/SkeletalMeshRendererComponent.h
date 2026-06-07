#pragma once
#include <string>
#include <vector>

#include "../base/BaseComponent.h"

#include "core/assets/AssetID.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;
	struct MaterialSlot;

	class SkeletalMeshRendererComponent final : public BaseComponent {
	public:
		// ---- SkeletalMeshRendererComponent ---------------------------------
		/// @brief メッシュのファイルパスを設定します。
		/// @param path メッシュのファイルパス。
		void SetMeshPath(const std::string& path);

		/// @brief マテリアルインスタンスのファイルパスを設定します。
		/// @param path マテリアルインスタンスのファイルパス。
		void SetMaterialInstancePath(const std::string& path);

		/// @brief マテリアルスロットを設定します。
		/// @param slots マテリアルスロットのベクタ。
		void SetMaterialSlots(const std::vector<MaterialSlot>& slots);

		/// @brief 指定されたスロットのマテリアルインスタンスのファイルパスを設定します。
		/// @param slotIndex スロットインデックス。
		/// @param path マテリアルインスタンスのファイルパス。
		void SetMaterialInstancePathForSlot(uint32_t slotIndex, const std::string& path);

		/// @brief メッシュのファイルパスを取得します。
		/// @return メッシュのファイルパス。
		[[nodiscard]] const std::string& GetMeshPath() const noexcept;

		/// @brief マテリアルインスタンスのファイルパスを取得します。
		/// @return マテリアルインスタンスのファイルパス。
		[[nodiscard]]
		const std::string& GetMaterialInstancePath() const noexcept;

		/// @brief マテリアルスロット一覧を取得します。
		/// @return マテリアルスロットのベクタ。
		[[nodiscard]] const std::vector<MaterialSlot>& GetMaterialSlots() const noexcept;

		/// @brief AssetManagerを使用してメッシュアセットIDを解決します。
		/// @param assetManager アセットマネージャーの参照。
		/// @return 解決されたメッシュアセットID。解決できない場合はkInvalidAssetID。
		AssetID ResolveMeshAsset(AssetManager& assetManager);

		/// @brief AssetManagerを使用してマテリアルインスタンスアセットIDを解決します。
		/// @param assetManager アセットマネージャーの参照。
		/// @return 解決されたマテリアルインスタンスアセットID。解決できない場合はkInvalidAssetID。
		AssetID ResolveMaterialInstanceAsset(AssetManager& assetManager);

		/// @brief AssetManagerを使用してすべてのマテリアルスロットのアセットIDを解決します。
		/// @param assetManager アセットマネージャーの参照。
		void ResolveMaterialInstanceAssets(AssetManager& assetManager);

		/// @brief メッシュのmaterialIndexに対応するマテリアルインスタンスアセットIDを解決します。
		/// @param assetManager アセットマネージャーの参照。
		/// @param materialIndex メッシュ側のmaterialIndex。
		/// @return 解決されたマテリアルインスタンスアセットID。解決できない場合はkInvalidAssetID。
		AssetID ResolveMaterialInstanceAssetForMaterialIndex(
			AssetManager& assetManager,
			uint32_t      materialIndex
		);

		/// @brief 現在のメッシュアセットIDを取得します。
		/// @return 現在のメッシュアセットID。解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMeshAssetId() const noexcept;

		/// @brief 現在のマテリアルインスタンスアセットIDを取得します。
		/// @return 現在のマテリアルインスタンスアセットID。解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetId() const noexcept;

		/// @brief 指定されたスロットのマテリアルインスタンスアセットIDを取得します。
		/// @param slotIndex スロットインデックス。
		/// @return マテリアルインスタンスアセットID。存在しない、または解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetIdForSlot(uint32_t slotIndex) const noexcept;

		/// @brief メッシュのmaterialIndexに対応するマテリアルインスタンスアセットIDを取得します。
		/// @param materialIndex メッシュ側のmaterialIndex。
		/// @return マテリアルインスタンスアセットID。存在しない、または解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetIdForMaterialIndex(uint32_t materialIndex) const noexcept;


		// ---- BaseComponent ------------------------------------------------
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const noexcept override;

	private:
		std::string mMeshPath;
		std::string mMaterialInstancePath;
		std::vector<MaterialSlot> mMaterialSlots;

		AssetID mMeshAssetId             = kInvalidAssetID;
		AssetID mMaterialInstanceAssetId = kInvalidAssetID;
		std::vector<AssetID> mMaterialInstanceAssetIds;  // スロット単位のマテリアルアセットID
	};
}
