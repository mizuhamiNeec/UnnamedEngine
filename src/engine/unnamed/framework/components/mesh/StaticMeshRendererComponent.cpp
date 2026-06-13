#include "StaticMeshRendererComponent.h"

#include <algorithm>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"

namespace Unnamed {
	namespace {
		std::string ReadStringOr(
			const JsonReader& reader, const char* key, const char* fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetString() : std::string(fallback);
		}

		uint32_t ComputeRequiredMaterialSlotCount(const MeshAssetData& meshAsset) {
			if (meshAsset.submeshes.empty()) {
				return 1;
			}

			uint32_t maxMaterialIndex = 0;
			for (const auto& submesh : meshAsset.submeshes) {
				maxMaterialIndex = std::max(maxMaterialIndex, submesh.materialIndex);
			}

			return maxMaterialIndex + 1;
		}
	}

	std::string_view StaticMeshRendererComponent::GetStableName() const {
		return "engine.StaticMeshRenderer";
	}

	std::string_view StaticMeshRendererComponent::GetComponentName() const {
		return "StaticMeshRenderer";
	}

	void StaticMeshRendererComponent::Deserialize(const JsonReader& reader) {
		std::string meshPath = ReadStringOr(reader, "meshPath", "");
		if (meshPath.empty()) {
			meshPath = ReadStringOr(reader, "mesh", "");
		}

		std::string matPath = ReadStringOr(
			reader, "materialInstancePath", ""
		);
		if (matPath.empty()) {
			matPath = ReadStringOr(reader, "material", "");
		}

		SetMeshPath(meshPath);

		// 新形式（materialSlots）をチェック
		const JsonReader slotsReader = reader["materialSlots"];
		if (slotsReader.Valid() && slotsReader.IsArray()) {
			std::vector<MaterialSlot> slots;
			for (size_t i = 0; i < slotsReader.Size(); ++i) {
				const JsonReader slotReader = slotsReader[i];
				if (!slotReader.Valid()) {
					continue;
				}
				MaterialSlot slot;
				slot.slotIndex = static_cast<uint32_t>(slotReader["slotIndex"].GetInt(static_cast<int>(i)));
				slot.materialInstancePath = slotReader["materialInstancePath"].GetString("");
				slots.push_back(slot);
			}
			SetMaterialSlots(slots);
		} else if (!matPath.empty()) {
			// 旧形式（単一 materialInstancePath）から新形式に変換
			SetMaterialInstancePath(matPath);
		}
	}

	void StaticMeshRendererComponent::Serialize(JsonWriter& writer) const {
		writer.Key("meshPath");
		writer.Write(mMeshPath);
		
		// 新形式で materialSlots を出力
		writer.Key("materialSlots");
		writer.BeginArray();
		for (const auto& slot : mMaterialSlots) {
			writer.BeginObject();
			writer.Key("slotIndex");
			writer.Write(slot.slotIndex);
			writer.Key("materialInstancePath");
			writer.Write(slot.materialInstancePath);
			writer.EndObject();
		}
		writer.EndArray();

		// 互換性のため古い形式も出力（mMaterialInstancePath が設定されている場合）
		writer.Key("materialInstancePath");
		writer.Write(mMaterialInstancePath);
	}

	uint32_t StaticMeshRendererComponent::GetIcon() const {
		return kIconDeployedCode;
	}

#ifdef _DEBUG
	void StaticMeshRendererComponent::DrawInspectorImGui() {
		std::string meshPath = mMeshPath;
		if (
			ImGuiWidgets::AssetPathPicker(
				"MeshPath",
				meshPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MESH)
			)
		) {
			SetMeshPath(meshPath);
		}

		uint32_t meshSlotCount = 0;
		if (AssetManager* assetManager = GetAssetManager()) {
			const AssetID meshAssetId = ResolveMeshAsset(*assetManager);
			if (meshAssetId != kInvalidAssetID) {
				const MeshAssetData* meshAsset = assetManager->Get<MeshAssetData>(meshAssetId);
				if (meshAsset != nullptr) {
					meshSlotCount = ComputeRequiredMaterialSlotCount(*meshAsset);
				}
			}
		}

		// Material Slots セクション
		if (ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (meshSlotCount > 0) {
				ImGui::Text("Mesh Slots: %u", meshSlotCount);
				if (ImGui::Button("Sync Slots From Mesh")) {
					std::vector<MaterialSlot> syncedSlots = mMaterialSlots;
					if (syncedSlots.size() < meshSlotCount) {
						syncedSlots.resize(meshSlotCount);
					}

					for (uint32_t i = 0; i < syncedSlots.size(); ++i) {
						syncedSlots[i].slotIndex = i;
					}

					SetMaterialSlots(syncedSlots);
				}
			}

			for (uint32_t i = 0; i < mMaterialSlots.size(); ++i) {
				std::string slotLabel = "Slot " + std::to_string(mMaterialSlots[i].slotIndex);
				std::string slotPath = mMaterialSlots[i].materialInstancePath;
				if (
					ImGuiWidgets::AssetPathPicker(
						slotLabel.c_str(),
						slotPath,
						ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MATERIAL_INSTANCE)
					)
				) {
					SetMaterialInstancePathForSlot(i, slotPath);
				}
			}
		}

		// 旧形式互換性：単一パスのピッカー（mMaterialSlots が空の場合のみ表示）
		if (mMaterialSlots.empty()) {
			std::string materialPath = mMaterialInstancePath;
			if (
				ImGuiWidgets::AssetPathPicker(
					"MaterialInstancePath",
					materialPath,
					ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MATERIAL_INSTANCE)
				)
			) {
				SetMaterialInstancePath(materialPath);
			}
		}
	}
#endif

	void StaticMeshRendererComponent::SetMeshPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMeshPath == normalized) {
			return;
		}

		mMeshPath    = normalized;
		mMeshAssetId = kInvalidAssetID;
	}

	void StaticMeshRendererComponent::SetMaterialInstancePath(
		const std::string& path
	) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMaterialInstancePath == normalized) {
			return;
		}

		mMaterialInstancePath    = normalized;
		mMaterialInstanceAssetId = kInvalidAssetID;
	}

	const std::string&
	StaticMeshRendererComponent::GetMeshPath() const noexcept {
		return mMeshPath;
	}

	const std::string&
	StaticMeshRendererComponent::GetMaterialInstancePath() const noexcept {
		return mMaterialInstancePath;
	}

	AssetID StaticMeshRendererComponent::ResolveMeshAsset(
		AssetManager& assetManager
	) {
		if (mMeshPath.empty()) {
			return kInvalidAssetID;
		}
		if (mMeshAssetId != kInvalidAssetID) {
			return mMeshAssetId;
		}

		mMeshAssetId = assetManager.LoadFromFile(mMeshPath, ASSET_TYPE::MESH);
		return mMeshAssetId;
	}

	AssetID StaticMeshRendererComponent::ResolveMaterialInstanceAsset(
		AssetManager& assetManager
	) {
		if (mMaterialInstancePath.empty()) {
			return kInvalidAssetID;
		}
		if (mMaterialInstanceAssetId != kInvalidAssetID) {
			return mMaterialInstanceAssetId;
		}

		mMaterialInstanceAssetId = assetManager.LoadFromFile(
			mMaterialInstancePath, ASSET_TYPE::MATERIAL_INSTANCE
		);
		return mMaterialInstanceAssetId;
	}

	AssetID StaticMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	StaticMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	void StaticMeshRendererComponent::SetMaterialSlots(
		const std::vector<MaterialSlot>& slots
	) {
		mMaterialSlots = slots;
		mMaterialInstanceAssetIds.clear();
		mMaterialInstanceAssetIds.resize(slots.size(), kInvalidAssetID);
	}

	void StaticMeshRendererComponent::SetMaterialInstancePathForSlot(
		uint32_t slotIndex, const std::string& path
	) {
		// スロットベクタが十分なサイズでない場合は拡張
		if (slotIndex >= mMaterialSlots.size()) {
			mMaterialSlots.resize(slotIndex + 1);
		}

		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMaterialSlots[slotIndex].materialInstancePath == normalized) {
			return;
		}

		mMaterialSlots[slotIndex].slotIndex = slotIndex;
		mMaterialSlots[slotIndex].materialInstancePath = normalized;

		// マテリアルアセットIDリセット
		if (slotIndex < mMaterialInstanceAssetIds.size()) {
			mMaterialInstanceAssetIds[slotIndex] = kInvalidAssetID;
		}
	}

	const std::vector<MaterialSlot>&
	StaticMeshRendererComponent::GetMaterialSlots() const noexcept {
		return mMaterialSlots;
	}

	void StaticMeshRendererComponent::ResolveMaterialInstanceAssets(
		AssetManager& assetManager
	) {
		// マテリアルスロットが空の場合、旧形式の単一パスで初期化
		if (mMaterialSlots.empty() && !mMaterialInstancePath.empty()) {
			MaterialSlot slot;
			slot.slotIndex = 0;
			slot.materialInstancePath = mMaterialInstancePath;
			mMaterialSlots.push_back(slot);
		}

		// マテリアルアセットIDリスト用意
		if (mMaterialInstanceAssetIds.size() != mMaterialSlots.size()) {
			mMaterialInstanceAssetIds.resize(mMaterialSlots.size(), kInvalidAssetID);
		}

		// 各スロットを解決
		for (uint32_t i = 0; i < mMaterialSlots.size(); ++i) {
			if (mMaterialInstanceAssetIds[i] != kInvalidAssetID) {
				continue;  // 既に解決済み
			}

			if (mMaterialSlots[i].materialInstancePath.empty()) {
				continue;  // パスが空の場合はスキップ
			}

			mMaterialInstanceAssetIds[i] = assetManager.LoadFromFile(
				mMaterialSlots[i].materialInstancePath, ASSET_TYPE::MATERIAL_INSTANCE
			);
		}
	}

	AssetID StaticMeshRendererComponent::ResolveMaterialInstanceAssetForMaterialIndex(
		AssetManager& assetManager,
		uint32_t      materialIndex
	) {
		ResolveMaterialInstanceAssets(assetManager);

		AssetID materialId = GetMaterialInstanceAssetIdForMaterialIndex(
			materialIndex
		);
		if (materialId != kInvalidAssetID) {
			return materialId;
		}

		// 旧形式の単一マテリアルとの互換性を維持します。
		return ResolveMaterialInstanceAsset(assetManager);
	}

	AssetID StaticMeshRendererComponent::GetMaterialInstanceAssetIdForSlot(
		uint32_t slotIndex
	) const noexcept {
		if (slotIndex >= mMaterialInstanceAssetIds.size()) {
			return kInvalidAssetID;
		}
		return mMaterialInstanceAssetIds[slotIndex];
	}

	AssetID
	StaticMeshRendererComponent::GetMaterialInstanceAssetIdForMaterialIndex(
		uint32_t materialIndex
	) const noexcept {
		AssetID materialId = GetMaterialInstanceAssetIdForSlot(materialIndex);
		if (materialId != kInvalidAssetID) {
			return materialId;
		}

		// materialIndexに一致するスロットが無い場合はslot0へフォールバック。
		materialId = GetMaterialInstanceAssetIdForSlot(0);
		if (materialId != kInvalidAssetID) {
			return materialId;
		}

		return mMaterialInstanceAssetId;
	}

	REGISTER_COMPONENT(StaticMeshRendererComponent);
}
