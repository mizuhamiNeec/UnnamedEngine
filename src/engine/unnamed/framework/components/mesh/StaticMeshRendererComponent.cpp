#include "StaticMeshRendererComponent.h"

#include <functional>

#include "engine/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] AssetID ResolveStoredVirtualAssetPath(
			const Path&                                       storedPath,
			const char*                                       assetKind,
			const std::function<AssetID(const VirtualPath&)>& loadVirtualPath
		) {
			const Path        normalizedPath = storedPath.LexicallyNormal();
			const std::string genericPath    = normalizedPath.ToGenericUtf8();
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(genericPath);
			if (!virtualPath.has_value()) {
				Error(
					"Scene",
					"Invalid {} virtual path: {}. Physical mesh/material paths are no longer supported in scene JSON.",
					assetKind,
					genericPath
				);
				return kInvalidAssetID;
			}

			return loadVirtualPath(*virtualPath);
		}
	}

	std::string_view StaticMeshRendererComponent::GetStableName() const {
		return "engine.StaticMeshRenderer";
	}

	std::string_view StaticMeshRendererComponent::GetComponentName() const {
		return "StaticMeshRenderer";
	}

	void StaticMeshRendererComponent::Deserialize(const JsonReader& reader) {
		constexpr SceneLoadOptions options{};
		const Path scenePath("<direct component deserialize>");
		const SceneDeserializeContext context{
			.loadOptions   = options,
			.assetManager  = GetAssetManager(),
			.scenePath     = scenePath,
			.entityName    = "<unknown>",
			.entityId      = 0,
			.componentType = GetStableName(),
		};
		(void)Deserialize(reader, context);
	}

	bool StaticMeshRendererComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		const bool strict        = IsStrictAssetValidation(context.loadOptions);
		mMeshAssetId             = kInvalidAssetID;
		mMaterialInstanceAssetId = kInvalidAssetID;
		SetMaterialSlots({});
		SetMaterialInstancePath({});
		std::string meshPath = reader.ReadStringOr("meshPath", "");
		if (meshPath.empty()) {
			meshPath = reader.ReadStringOr("mesh", "");
		}

		std::string matPath = reader.ReadStringOr(
			"materialInstancePath", ""
		);
		if (matPath.empty()) {
			matPath = reader.ReadStringOr("material", "");
		}

		if (meshPath.empty()) {
			SetMeshPath({});
		} else if (
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(meshPath);
			virtualPath.has_value()
		) {
			SetMeshPath(Path(virtualPath->String()));
			if (context.assetManager != nullptr) {
				const std::optional<ResolvedContentFile> resolvedFile =
					context.assetManager->GetContentPathResolver().ResolveFile(
						*virtualPath
					);
				if (!resolvedFile.has_value()) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}'",
						context.scenePath,
						context.entityName,
						context.entityId,
						context.componentType,
						meshPath
					);
					SetMeshPath({});
					if (strict) {
						return false;
					}
				} else {
					mMeshAssetId = context.assetManager->LoadMesh(*virtualPath);
					if (mMeshAssetId == kInvalidAssetID) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}' mount='{}' physicalPath='{}'",
							context.scenePath,
							context.entityName,
							context.entityId,
							context.componentType,
							meshPath,
							resolvedFile->mountId,
							resolvedFile->resolvedPath
						);
						SetMeshPath({});
						if (strict) {
							return false;
						}
					}
				}
			} else if (strict) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}' reason='AssetManager unavailable'",
					context.scenePath,
					context.entityName,
					context.entityId,
					context.componentType,
					meshPath
				);
				SetMeshPath({});
				return false;
			}
		} else {
			Error(
				"Scene",
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				meshPath
			);
			SetMeshPath({});
			if (strict) {
				return false;
			}
		}

		// 新形式（materialSlots）をチェック
		const JsonReader slotsReader = reader["materialSlots"];
		if (slotsReader.Valid() && slotsReader.IsArray()) {
			std::vector<MaterialSlot> slots;
			std::vector<AssetID>      materialAssetIds;
			for (size_t i = 0; i < slotsReader.Size(); ++i) {
				const JsonReader slotReader = slotsReader[i];
				if (!slotReader.Valid()) {
					continue;
				}
				MaterialSlot slot;
				slot.slotIndex = static_cast<uint32_t>(slotReader["slotIndex"].
					GetInt(static_cast<int>(i)));
				const std::string slotMaterialPath =
					slotReader["materialInstancePath"].GetString("");
				AssetID materialAssetId = kInvalidAssetID;
				if (slotMaterialPath.empty()) {
					slot.materialInstancePath = {};
				} else if (
					const std::optional<VirtualPath> virtualPath =
						VirtualPath::ParseContentReference(slotMaterialPath);
					virtualPath.has_value()
				) {
					slot.materialInstancePath = Path(virtualPath->String());
					if (context.assetManager != nullptr) {
						const std::optional<ResolvedContentFile> resolvedFile =
							context.assetManager->GetContentPathResolver().
							        ResolveFile(
								        *virtualPath
							        );
						if (!resolvedFile.has_value()) {
							Error(
								"Scene",
								"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}'",
								context.scenePath,
								context.entityName,
								context.entityId,
								context.componentType,
								i,
								slotMaterialPath
							);
							slot.materialInstancePath = {};
							if (strict) {
								return false;
							}
						} else {
							materialAssetId = context.assetManager->
								LoadMaterialInstance(*virtualPath);
							if (materialAssetId == kInvalidAssetID) {
								Error(
									"Scene",
									"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}' mount='{}' physicalPath='{}'",
									context.scenePath,
									context.entityName,
									context.entityId,
									context.componentType,
									i,
									slotMaterialPath,
									resolvedFile->mountId,
									resolvedFile->resolvedPath
								);
								slot.materialInstancePath = {};
								if (strict) {
									return false;
								}
							}
						}
					} else if (strict) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}' reason='AssetManager unavailable'",
							context.scenePath,
							context.entityName,
							context.entityId,
							context.componentType,
							i,
							slotMaterialPath
						);
						return false;
					}
				} else {
					Error(
						"Scene",
						"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}'",
						context.scenePath,
						context.entityName,
						context.entityId,
						context.componentType,
						i,
						slotMaterialPath
					);
					slot.materialInstancePath = {};
					if (strict) {
						return false;
					}
				}
				slots.push_back(slot);
				materialAssetIds.push_back(materialAssetId);
			}
			SetMaterialSlots(slots);
			mMaterialInstanceAssetIds = std::move(materialAssetIds);
		} else if (matPath.empty()) {
			SetMaterialInstancePath({});
		} else if (
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(matPath);
			virtualPath.has_value()
		) {
			// 旧形式（単一 materialInstancePath）から新形式に変換
			SetMaterialInstancePath(Path(virtualPath->String()));
			if (context.assetManager != nullptr) {
				const std::optional<ResolvedContentFile> resolvedFile =
					context.assetManager->GetContentPathResolver().ResolveFile(
						*virtualPath
					);
				if (!resolvedFile.has_value()) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}'",
						context.scenePath,
						context.entityName,
						context.entityId,
						context.componentType,
						matPath
					);
					SetMaterialInstancePath({});
					if (strict) {
						return false;
					}
				} else {
					mMaterialInstanceAssetId = context.assetManager->
						LoadMaterialInstance(*virtualPath);
					if (mMaterialInstanceAssetId == kInvalidAssetID) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}' mount='{}' physicalPath='{}'",
							context.scenePath,
							context.entityName,
							context.entityId,
							context.componentType,
							matPath,
							resolvedFile->mountId,
							resolvedFile->resolvedPath
						);
						SetMaterialInstancePath({});
						if (strict) {
							return false;
						}
					}
				}
			} else if (strict) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}' reason='AssetManager unavailable'",
					context.scenePath,
					context.entityName,
					context.entityId,
					context.componentType,
					matPath
				);
				SetMaterialInstancePath({});
				return false;
			}
		} else {
			Error(
				"Scene",
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				matPath
			);
			SetMaterialInstancePath({});
			if (strict) {
				return false;
			}
		}

		return true;
	}

	void StaticMeshRendererComponent::Serialize(JsonWriter& writer) const {
		writer.Key("meshPath");
		writer.Write(mMeshPath.ToGenericUtf8());

		// 新形式で materialSlots を出力
		writer.Key("materialSlots");
		writer.BeginArray();
		for (const auto& slot : mMaterialSlots) {
			writer.BeginObject();
			writer.Key("slotIndex");
			writer.Write(slot.slotIndex);
			writer.Key("materialInstancePath");
			writer.Write(slot.materialInstancePath.ToGenericUtf8());
			writer.EndObject();
		}
		writer.EndArray();

		// 互換性のため古い形式も出力（mMaterialInstancePath が設定されている場合）
		writer.Key("materialInstancePath");
		writer.Write(mMaterialInstancePath.ToGenericUtf8());
	}

	uint32_t StaticMeshRendererComponent::GetIcon() const {
		return kIconDeployedCode;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void StaticMeshRendererComponent::DrawInspectorImGui() {
		std::string meshPath = mMeshPath.ToGenericUtf8();
		if (
			ImGuiWidgets::AssetPathPicker(
				"MeshPath",
				meshPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MESH)
			)
		) {
			SetMeshPath(Path(meshPath));
		}

		uint32_t meshSlotCount = 0;
		if (AssetManager* assetManager = GetAssetManager()) {
			const AssetID meshAssetId = ResolveMeshAsset(*assetManager);
			if (meshAssetId != kInvalidAssetID) {
				const MeshAssetData* meshAsset = assetManager->Get<
					MeshAssetData>(meshAssetId);
				if (meshAsset != nullptr) {
					meshSlotCount =
						ComputeRequiredMaterialSlotCount(*meshAsset);
				}
			}
		}

		// Material Slots セクション
		if (ImGui::CollapsingHeader("Material Slots",
		                            ImGuiTreeNodeFlags_DefaultOpen)) {
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
				std::string slotLabel = "Slot " + std::to_string(
					                        mMaterialSlots[i].slotIndex);
				std::string slotPath = mMaterialSlots[i].materialInstancePath.
					ToGenericUtf8();
				if (
					ImGuiWidgets::AssetPathPicker(
						slotLabel.c_str(),
						slotPath,
						ImGuiWidgets::AssetTypeToMask(
							ASSET_TYPE::MATERIAL_INSTANCE)
					)
				) {
					SetMaterialInstancePathForSlot(i, Path(slotPath));
				}
			}
		}

		// 旧形式互換性：単一パスのピッカー（mMaterialSlots が空の場合のみ表示）
		if (mMaterialSlots.empty()) {
			std::string materialPath = mMaterialInstancePath.ToGenericUtf8();
			if (
				ImGuiWidgets::AssetPathPicker(
					"MaterialInstancePath",
					materialPath,
					ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MATERIAL_INSTANCE)
				)
			) {
				SetMaterialInstancePath(Path(materialPath));
			}
		}
	}
#endif

	void StaticMeshRendererComponent::SetMeshPath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mMeshPath == path) {
			return;
		}

		mMeshPath    = std::move(path);
		mMeshAssetId = kInvalidAssetID;
	}

	void StaticMeshRendererComponent::SetMaterialInstancePath(
		Path path
	) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mMaterialInstancePath == path) {
			return;
		}

		mMaterialInstancePath    = std::move(path);
		mMaterialInstanceAssetId = kInvalidAssetID;
	}

	void StaticMeshRendererComponent::SetMaterialSlots(
		const std::vector<MaterialSlot>& slots
	) {
		mMaterialSlots = slots;
		mMaterialInstanceAssetIds.clear();
		mMaterialInstanceAssetIds.resize(slots.size(), kInvalidAssetID);
	}

	void StaticMeshRendererComponent::SetMaterialInstancePathForSlot(
		const uint32_t slotIndex, Path path
	) {
		// スロットベクタが十分なサイズでない場合は拡張
		if (slotIndex >= mMaterialSlots.size()) {
			mMaterialSlots.resize(slotIndex + 1);
		}

		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mMaterialSlots[slotIndex].materialInstancePath == path) {
			return;
		}

		mMaterialSlots[slotIndex].slotIndex            = slotIndex;
		mMaterialSlots[slotIndex].materialInstancePath = std::move(path);

		// マテリアルアセットIDリセット
		if (slotIndex < mMaterialInstanceAssetIds.size()) {
			mMaterialInstanceAssetIds[slotIndex] = kInvalidAssetID;
		}
	}

	const Path& StaticMeshRendererComponent::GetMeshPath() const noexcept {
		return mMeshPath;
	}

	const Path&
	StaticMeshRendererComponent::GetMaterialInstancePath() const noexcept {
		return mMaterialInstancePath;
	}

	const std::vector<MaterialSlot>&
	StaticMeshRendererComponent::GetMaterialSlots() const noexcept {
		return mMaterialSlots;
	}

	AssetID StaticMeshRendererComponent::ResolveMeshAsset(
		AssetManager& assetManager
	) {
		if (mMeshPath.IsEmpty()) {
			return kInvalidAssetID;
		}
		if (mMeshAssetId != kInvalidAssetID) {
			return mMeshAssetId;
		}

		mMeshAssetId = ResolveStoredVirtualAssetPath(
			mMeshPath,
			"mesh",
			[&assetManager](const VirtualPath& virtualPath) {
				return assetManager.LoadMesh(virtualPath);
			}
		);
		return mMeshAssetId;
	}

	AssetID StaticMeshRendererComponent::ResolveMaterialInstanceAsset(
		AssetManager& assetManager
	) {
		if (mMaterialInstancePath.IsEmpty()) {
			return kInvalidAssetID;
		}
		if (mMaterialInstanceAssetId != kInvalidAssetID) {
			return mMaterialInstanceAssetId;
		}

		mMaterialInstanceAssetId = ResolveStoredVirtualAssetPath(
			mMaterialInstancePath,
			"material instance",
			[&assetManager](const VirtualPath& virtualPath) {
				return assetManager.LoadMaterialInstance(virtualPath);
			}
		);
		return mMaterialInstanceAssetId;
	}

	void StaticMeshRendererComponent::ResolveMaterialInstanceAssets(
		AssetManager& assetManager
	) {
		// マテリアルスロットが空の場合、旧形式の単一パスで初期化
		if (mMaterialSlots.empty() && !mMaterialInstancePath.IsEmpty()) {
			MaterialSlot slot;
			slot.slotIndex            = 0;
			slot.materialInstancePath = mMaterialInstancePath;
			mMaterialSlots.push_back(slot);
		}

		// マテリアルアセットIDリスト用意
		if (mMaterialInstanceAssetIds.size() != mMaterialSlots.size()) {
			mMaterialInstanceAssetIds.resize(mMaterialSlots.size(),
			                                 kInvalidAssetID);
		}

		// 各スロットを解決
		for (uint32_t i = 0; i < mMaterialSlots.size(); ++i) {
			if (mMaterialInstanceAssetIds[i] != kInvalidAssetID) {
				continue; // 既に解決済み
			}

			if (mMaterialSlots[i].materialInstancePath.IsEmpty()) {
				continue; // パスが空の場合はスキップ
			}

			mMaterialInstanceAssetIds[i] = ResolveStoredVirtualAssetPath(
				mMaterialSlots[i].materialInstancePath,
				"material instance",
				[&assetManager](const VirtualPath& virtualPath) {
					return assetManager.LoadMaterialInstance(virtualPath);
				}
			);
		}
	}

	AssetID
	StaticMeshRendererComponent::ResolveMaterialInstanceAssetForMaterialIndex(
		AssetManager& assetManager,
		const uint32_t      materialIndex
	) {
		ResolveMaterialInstanceAssets(assetManager);

		const AssetID materialId = GetMaterialInstanceAssetIdForMaterialIndex(
			materialIndex
		);
		if (materialId != kInvalidAssetID) {
			return materialId;
		}

		// 旧形式の単一マテリアルとの互換性を維持します。
		return ResolveMaterialInstanceAsset(assetManager);
	}

	AssetID StaticMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	StaticMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	AssetID StaticMeshRendererComponent::GetMaterialInstanceAssetIdForSlot(
		const uint32_t slotIndex
	) const noexcept {
		if (slotIndex >= mMaterialInstanceAssetIds.size()) {
			return kInvalidAssetID;
		}
		return mMaterialInstanceAssetIds[slotIndex];
	}

	AssetID
	StaticMeshRendererComponent::GetMaterialInstanceAssetIdForMaterialIndex(
		const uint32_t materialIndex
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
