#include "SkeletalMeshRendererComponent.h"

#include <algorithm>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneLoadOptions.h"

namespace Unnamed {
	namespace {
		uint32_t ComputeRequiredMaterialSlotCount(
			const MeshAssetData& meshAsset
		) {
			if (meshAsset.submeshes.empty()) {
				return 1;
			}

			uint32_t maxMaterialIndex = 0;
			for (const auto& submesh : meshAsset.submeshes) {
				maxMaterialIndex = std::max(maxMaterialIndex,
				                            submesh.materialIndex);
			}

			return maxMaterialIndex + 1;
		}
	}

	void SkeletalMeshRendererComponent::OnAttached() {
		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			return;
		}
		if (mMeshPath.has_value() && mMeshAssetId == kInvalidAssetID) {
			const VirtualPath path = *mMeshPath;
			(void)SetMeshPath(path, *assetManager);
		}
		if (
			mMaterialInstancePath.has_value() &&
			mMaterialInstanceAssetId == kInvalidAssetID
		) {
			const VirtualPath path = *mMaterialInstancePath;
			(void)SetMaterialInstancePath(path, *assetManager);
		}
		for (SkeletalMaterialSlotReference& slot : mMaterialSlots) {
			if (!slot.materialInstancePath.has_value() ||
			    slot.assetId != kInvalidAssetID) {
				continue;
			}
			const VirtualPath path = *slot.materialInstancePath;
			(void)SetMaterialInstancePathForSlot(
				slot.slotIndex, path, *assetManager
			);
		}
	}

	std::string_view SkeletalMeshRendererComponent::GetStableName() const {
		return "engine.SkeletalMeshRenderer";
	}

	std::string_view SkeletalMeshRendererComponent::GetComponentName() const {
		return "SkeletalMeshRenderer";
	}

	void SkeletalMeshRendererComponent::Deserialize(const JsonReader& reader) {
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

	bool SkeletalMeshRendererComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		ClearMeshPath();
		ClearMaterialInstancePath();
		ClearMaterialSlots();
		const bool strict = IsStrictAssetValidation(context.loadOptions);

		const JsonReader meshNode = reader["meshPath"].Valid() ?
			reader["meshPath"] : reader["mesh"];
		if (meshNode.Valid()) {
			if (!meshNode.IsString()) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' reason='expected string'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType
				);
				if (strict) {
					return false;
				}
			} else if (const std::string value = meshNode.GetString(); !value.empty()) {
				const std::optional<VirtualPath> path =
					VirtualPath::ParseContentReference(value);
				if (!path.has_value()) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}'",
						context.scenePath, context.entityName, context.entityId,
						context.componentType, value
					);
					if (strict) {
						return false;
					}
				} else if (!context.assetManager) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}' reason='AssetManager unavailable'",
						context.scenePath, context.entityName, context.entityId,
						context.componentType, value
					);
					if (strict) {
						return false;
					}
				} else {
					const std::optional<ResolvedContentFile> resolved =
						context.assetManager->GetContentPathResolver().ResolveFile(*path);
					if (!resolved.has_value()) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}'",
							context.scenePath, context.entityName, context.entityId,
							context.componentType, value
						);
						if (strict) {
							return false;
						}
					} else if (!SetMeshPath(*path, *context.assetManager)) {
						const AssetID loadedId = context.assetManager->FindByPath(
							resolved->resolvedPath
						);
						const bool contentsMismatch = loadedId != kInvalidAssetID &&
							context.assetManager->Get<MeshAssetData>(loadedId) != nullptr;
						Error(
							"Scene",
							"Scene asset reference failed: classification='{}' scene='{}' entity='{}' entityId={} component='{}' field='meshPath' virtualPath='{}' mount='{}' physicalPath='{}'",
							contentsMismatch ? "mesh contents mismatch" :
							"asset load failure",
							context.scenePath, context.entityName, context.entityId,
							context.componentType, value, resolved->mountId,
							resolved->resolvedPath
						);
						if (strict) {
							return false;
						}
					}
				}
			}
		}

		const JsonReader materialNode = reader["materialInstancePath"].Valid() ?
			reader["materialInstancePath"] : reader["material"];
		if (materialNode.Valid()) {
			if (!materialNode.IsString()) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' reason='expected string'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType
				);
				if (strict) {
					return false;
				}
			} else if (const std::string value = materialNode.GetString(); !value.empty()) {
				const std::optional<VirtualPath> path =
					VirtualPath::ParseContentReference(value);
				if (!path.has_value()) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}'",
						context.scenePath, context.entityName, context.entityId,
						context.componentType, value
					);
					if (strict) {
						return false;
					}
				} else if (!context.assetManager) {
					Error(
						"Scene",
						"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}' reason='AssetManager unavailable'",
						context.scenePath, context.entityName, context.entityId,
						context.componentType, value
					);
					if (strict) {
						return false;
					}
				} else {
					const std::optional<ResolvedContentFile> resolved =
						context.assetManager->GetContentPathResolver().ResolveFile(*path);
					if (!resolved.has_value()) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}'",
							context.scenePath, context.entityName, context.entityId,
							context.componentType, value
						);
						if (strict) {
							return false;
						}
					} else if (!SetMaterialInstancePath(*path, *context.assetManager)) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialInstancePath' virtualPath='{}' mount='{}' physicalPath='{}'",
							context.scenePath, context.entityName, context.entityId,
							context.componentType, value, resolved->mountId,
							resolved->resolvedPath
						);
						if (strict) {
							return false;
						}
					}
				}
			}
		}

		const JsonReader slotsNode = reader["materialSlots"];
		if (!slotsNode.Valid()) {
			return true;
		}
		if (!slotsNode.IsArray()) {
			Error(
				"Scene",
				"Scene asset reference failed: classification='invalid material slots' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots' reason='expected array'",
				context.scenePath, context.entityName, context.entityId,
				context.componentType
			);
			return !strict;
		}

		for (size_t i = 0; i < slotsNode.Size(); ++i) {
			const JsonReader slotNode = slotsNode[i];
			uint32_t slotIndex = static_cast<uint32_t>(i);
			JsonReader pathNode = slotNode;
			if (slotNode.IsObject()) {
				const JsonReader indexNode = slotNode["slotIndex"];
				if (indexNode.Valid()) {
					const int index = indexNode.GetInt(-1);
					if (!indexNode.IsNumberInteger() || index < 0) {
						Error(
							"Scene",
							"Scene asset reference failed: classification='invalid material slot' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].slotIndex' reason='expected non-negative integer'",
							context.scenePath, context.entityName,
							context.entityId, context.componentType, i
						);
						ClearMaterialSlots();
						return !strict;
					}
					slotIndex = static_cast<uint32_t>(index);
				}
				pathNode = slotNode["materialInstancePath"];
			}
			if (!pathNode.Valid()) {
				mMaterialSlots.emplace_back(
					SkeletalMaterialSlotReference{.slotIndex = slotIndex}
				);
				continue;
			}
			if (!pathNode.IsString()) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='invalid material slot' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}]' reason='expected string or object'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType, i
				);
				ClearMaterialSlots();
				return !strict;
			}
			const std::string value = pathNode.GetString();
			if (value.empty()) {
				mMaterialSlots.emplace_back(
					SkeletalMaterialSlotReference{.slotIndex = slotIndex}
				);
				continue;
			}
			const std::optional<VirtualPath> path =
				VirtualPath::ParseContentReference(value);
			if (!path.has_value()) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType, i, value
				);
				ClearMaterialSlots();
				return !strict;
			}
			if (!context.assetManager) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}' reason='AssetManager unavailable'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType, i, value
				);
				ClearMaterialSlots();
				return !strict;
			}
			const std::optional<ResolvedContentFile> resolved =
				context.assetManager->GetContentPathResolver().ResolveFile(*path);
			if (!resolved.has_value()) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType, i, value
				);
				ClearMaterialSlots();
				return !strict;
			}
			if (!SetMaterialInstancePathForSlot(
				slotIndex, *path, *context.assetManager
			)) {
				Error(
					"Scene",
					"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='materialSlots[{}].materialInstancePath' virtualPath='{}' mount='{}' physicalPath='{}'",
					context.scenePath, context.entityName, context.entityId,
					context.componentType, i, value, resolved->mountId,
					resolved->resolvedPath
				);
				ClearMaterialSlots();
				return !strict;
			}
		}
		return true;
	}

	void SkeletalMeshRendererComponent::Serialize(JsonWriter& writer) const {
		if (mMeshPath.has_value()) {
			writer.Key("meshPath");
			writer.Write(mMeshPath->String());
		}

		// 新形式で materialSlots を出力
		writer.Key("materialSlots");
		writer.BeginArray();
		for (const SkeletalMaterialSlotReference& slot : mMaterialSlots) {
			writer.BeginObject();
			writer.Key("slotIndex");
			writer.Write(slot.slotIndex);
			if (slot.materialInstancePath.has_value()) {
				writer.Key("materialInstancePath");
				writer.Write(slot.materialInstancePath->String());
			}
			writer.EndObject();
		}
		writer.EndArray();

		if (mMaterialInstancePath.has_value()) {
			writer.Key("materialInstancePath");
			writer.Write(mMaterialInstancePath->String());
		}
	}

	uint32_t SkeletalMeshRendererComponent::GetIcon() const noexcept {
		return kIconAccessibility;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkeletalMeshRendererComponent::DrawInspectorImGui() {
		std::string meshPath = mMeshPath.has_value() ?
			mMeshPath->String() : std::string{};
		if (
			ImGuiWidgets::AssetPathPicker(
				"SkeletalMeshPath",
				meshPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MESH)
			)
		) {
			const std::optional<VirtualPath> path =
				VirtualPath::ParseContentReference(meshPath);
			AssetManager* assetManager = GetAssetManager();
			if (!path.has_value() || !assetManager) {
				ClearMeshPath();
			} else {
				(void)SetMeshPath(*path, *assetManager);
			}
		}

		uint32_t meshSlotCount = 0;
		if (AssetManager* assetManager = GetAssetManager()) {
			const AssetID meshAssetId = GetMeshAssetId();
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
					std::vector<SkeletalMaterialSlotReference> syncedSlots =
						mMaterialSlots;
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
				std::string slotPath =
					mMaterialSlots[i].materialInstancePath.has_value() ?
					mMaterialSlots[i].materialInstancePath->String() :
					std::string{};
				if (
					ImGuiWidgets::AssetPathPicker(
						slotLabel.c_str(),
						slotPath,
						ImGuiWidgets::AssetTypeToMask(
							ASSET_TYPE::MATERIAL_INSTANCE)
					)
				) {
					const std::optional<VirtualPath> path =
						VirtualPath::ParseContentReference(slotPath);
					AssetManager* assetManager = GetAssetManager();
					if (!path.has_value() || !assetManager) {
						mMaterialSlots[i].materialInstancePath.reset();
						mMaterialSlots[i].assetId = kInvalidAssetID;
				} else {
					(void)SetMaterialInstancePathForSlot(
						mMaterialSlots[i].slotIndex, *path, *assetManager
					);
					}
				}
			}
		}

		// 旧形式互換性：単一パスのピッカー（mMaterialSlots が空の場合のみ表示）
		if (mMaterialSlots.empty()) {
			std::string materialPath = mMaterialInstancePath.has_value() ?
				mMaterialInstancePath->String() : std::string{};
			if (
				ImGuiWidgets::AssetPathPicker(
					"SkeletalMaterialPath",
					materialPath,
					ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MATERIAL_INSTANCE)
				)
			) {
				const std::optional<VirtualPath> path =
					VirtualPath::ParseContentReference(materialPath);
				AssetManager* assetManager = GetAssetManager();
				if (!path.has_value() || !assetManager) {
					ClearMaterialInstancePath();
				} else {
					(void)SetMaterialInstancePath(*path, *assetManager);
				}
			}
		}
	}
#endif

	bool SkeletalMeshRendererComponent::SetMeshPath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (mMeshPath.has_value() && *mMeshPath == path &&
		    mMeshAssetId != kInvalidAssetID) {
			return true;
		}

		const AssetID meshAssetId = assetManager.LoadMesh(path);
		const MeshAssetData* meshAsset = assetManager.Get<MeshAssetData>(
			meshAssetId
		);
		const bool hasWeightedVertex = meshAsset && std::ranges::any_of(
			meshAsset->vertices,
			[](const MeshVertex& vertex) {
				return std::ranges::any_of(
					vertex.boneWeights,
					[](const float weight) { return weight > 0.0f; }
				);
			}
		);
		if (!meshAsset || !meshAsset->hasSkinning ||
		    meshAsset->skeleton.empty() || !hasWeightedVertex) {
			if (meshAssetId != kInvalidAssetID) {
				Error(
					"SkeletalMesh",
					"Mesh asset is not suitable for skeletal rendering: virtualPath={} assetId={} hasSkinning={} boneCount={} hasWeightedVertex={}",
					path.String(), meshAssetId,
					meshAsset ? meshAsset->hasSkinning : false,
					meshAsset ? meshAsset->skeleton.size() : 0,
					hasWeightedVertex
				);
			}
			ClearMeshPath();
			return false;
		}

		mMeshPath    = path;
		mMeshAssetId = meshAssetId;
		return true;
	}

	void SkeletalMeshRendererComponent::ClearMeshPath() noexcept {
		mMeshPath.reset();
		mMeshAssetId = kInvalidAssetID;
	}

	bool SkeletalMeshRendererComponent::SetMaterialInstancePath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (mMaterialInstancePath.has_value() &&
		    *mMaterialInstancePath == path &&
		    mMaterialInstanceAssetId != kInvalidAssetID) {
			return true;
		}

		const AssetID assetId = assetManager.LoadMaterialInstance(path);
		if (assetId == kInvalidAssetID) {
			ClearMaterialInstancePath();
			return false;
		}
		mMaterialInstancePath    = path;
		mMaterialInstanceAssetId = assetId;
		return true;
	}

	void SkeletalMeshRendererComponent::ClearMaterialInstancePath() noexcept {
		mMaterialInstancePath.reset();
		mMaterialInstanceAssetId = kInvalidAssetID;
	}

	void SkeletalMeshRendererComponent::SetMaterialSlots(
		const std::vector<SkeletalMaterialSlotReference>& slots
	) {
		mMaterialSlots = slots;
	}

	void SkeletalMeshRendererComponent::ClearMaterialSlots() noexcept {
		mMaterialSlots.clear();
	}

	bool SkeletalMeshRendererComponent::SetMaterialInstancePathForSlot(
		const uint32_t slotIndex, const VirtualPath& path,
		AssetManager& assetManager
	) {
		const AssetID assetId = assetManager.LoadMaterialInstance(path);
		if (assetId == kInvalidAssetID) {
			const auto failedSlot = std::ranges::find(
				mMaterialSlots, slotIndex,
				&SkeletalMaterialSlotReference::slotIndex
			);
			if (failedSlot != mMaterialSlots.end()) {
				failedSlot->materialInstancePath.reset();
				failedSlot->assetId = kInvalidAssetID;
			}
			return false;
		}
		const auto slot = std::ranges::find(
			mMaterialSlots, slotIndex,
			&SkeletalMaterialSlotReference::slotIndex
		);
		if (slot != mMaterialSlots.end()) {
			slot->materialInstancePath = path;
			slot->assetId              = assetId;
		} else {
			mMaterialSlots.emplace_back(
				SkeletalMaterialSlotReference{
					.slotIndex            = slotIndex,
					.materialInstancePath = path,
					.assetId               = assetId,
				}
			);
		}
		return true;
	}

	const std::optional<VirtualPath>&
	SkeletalMeshRendererComponent::GetMeshPath() const noexcept {
		return mMeshPath;
	}

	const std::optional<VirtualPath>&
	SkeletalMeshRendererComponent::GetMaterialInstancePath() const noexcept {
		return mMaterialInstancePath;
	}

	const std::vector<SkeletalMaterialSlotReference>&
	SkeletalMeshRendererComponent::GetMaterialSlots() const noexcept {
		return mMaterialSlots;
	}

	AssetID SkeletalMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	SkeletalMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	AssetID SkeletalMeshRendererComponent::GetMaterialInstanceAssetIdForSlot(
		const uint32_t slotIndex
	) const noexcept {
		const auto slot = std::ranges::find(
			mMaterialSlots, slotIndex,
			&SkeletalMaterialSlotReference::slotIndex
		);
		return slot != mMaterialSlots.end() ? slot->assetId : kInvalidAssetID;
	}

	AssetID
	SkeletalMeshRendererComponent::GetMaterialInstanceAssetIdForMaterialIndex(
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

	REGISTER_COMPONENT(SkeletalMeshRendererComponent);
}
