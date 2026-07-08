#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../base/BaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;
	/// @brief Skeletal MeshのMaterial slot参照です。
	struct SkeletalMaterialSlotReference final {
		uint32_t                   slotIndex = 0;
		std::optional<VirtualPath> materialInstancePath;
		AssetID                    assetId = kInvalidAssetID;
	};

	class SkeletalMeshRendererComponent final : public BaseComponent {
	public:
		// ---- SkeletalMeshRendererComponent ---------------------------------
		/// @brief Skeletal Mesh参照をロードして設定します。
		[[nodiscard]] bool SetMeshPath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief Skeletal Mesh参照をクリアします。
		void ClearMeshPath() noexcept;

		/// @brief 単一Material Instance参照をロードして設定します。
		[[nodiscard]] bool SetMaterialInstancePath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief 単一Material Instance参照をクリアします。
		void ClearMaterialInstancePath() noexcept;

		/// @brief マテリアルスロットを設定します。
		/// @param slots マテリアルスロットのベクタ。
		void SetMaterialSlots(
			const std::vector<SkeletalMaterialSlotReference>& slots
		);
		/// @brief マテリアルスロットをクリアします。
		void ClearMaterialSlots() noexcept;

		/// @brief 指定slotのMaterial Instance参照をロードして設定します。
		/// @param slotIndex スロットインデックス。
		/// @param path Material Instanceの論理パス。
		/// @param assetManager ロードに使用するAssetManager。
		[[nodiscard]] bool SetMaterialInstancePathForSlot(
			uint32_t slotIndex, const VirtualPath& path,
			AssetManager& assetManager
		);

		/// @brief Skeletal Meshの論理パスを取得します。
		[[nodiscard]] const std::optional<VirtualPath>& GetMeshPath()
		const noexcept;

		/// @brief 単一Material Instanceの論理パスを取得します。
		[[nodiscard]]
		const std::optional<VirtualPath>& GetMaterialInstancePath() const noexcept;

		/// @brief マテリアルスロット一覧を取得します。
		/// @return マテリアルスロットのベクタ。
		[[nodiscard]] const std::vector<SkeletalMaterialSlotReference>&
		GetMaterialSlots() const noexcept;

		/// @brief 現在のメッシュアセットIDを取得します。
		/// @return 現在のメッシュアセットID。解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMeshAssetId() const noexcept;

		/// @brief 現在のマテリアルインスタンスアセットIDを取得します。
		/// @return 現在のマテリアルインスタンスアセットID。解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetId() const noexcept;

		/// @brief 指定されたスロットのマテリアルインスタンスアセットIDを取得します。
		/// @param slotIndex スロットインデックス。
		/// @return マテリアルインスタンスアセットID。存在しない、または解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetIdForSlot(
			uint32_t slotIndex
		) const noexcept;

		/// @brief メッシュのmaterialIndexに対応するマテリアルインスタンスアセットIDを取得します。
		/// @param materialIndex メッシュ側のmaterialIndex。
		/// @return マテリアルインスタンスアセットID。存在しない、または解決されていない場合はkInvalidAssetID。
		[[nodiscard]] AssetID GetMaterialInstanceAssetIdForMaterialIndex(
			uint32_t materialIndex
		) const noexcept;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const noexcept override;

	private:
		std::optional<VirtualPath> mMeshPath;
		std::optional<VirtualPath> mMaterialInstancePath;
		std::vector<SkeletalMaterialSlotReference> mMaterialSlots;

		AssetID mMeshAssetId             = kInvalidAssetID;
		AssetID mMaterialInstanceAssetId = kInvalidAssetID;
	};
}
