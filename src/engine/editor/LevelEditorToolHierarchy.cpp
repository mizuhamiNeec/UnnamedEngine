#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <imgui.h>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "engine/ComponentRegistry.h"
#include "core/guidgenerator/GuidGenerator.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/editor/sequence/SequenceEditorController.h"
#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiUtil.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneFolderPath.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		/// @brief OutlinerFolderNodeは、Outliner folder名、親子関係、展開状態を保持します
		struct OutlinerFolderNode {
			std::map<std::string, OutlinerFolderNode> children;
			std::vector<Entity*>                      entities;
		};

		/// @brief ComponentMenuNodeは、add-component menuのcategory階層と登録component項目を保持します
		struct ComponentMenuNode {
			std::map<std::string, ComponentMenuNode>                children;
			std::vector<ComponentRegistry::RegisteredComponentInfo> components;
		};

		bool IsValidComponentStableNameForMenu(
			const std::string_view stableName
		) {
			if (stableName.empty()) {
				return false;
			}
			if (stableName.front() == '.' || stableName.back() == '.') {
				return false;
			}
			return true;
		}

		bool InsertComponentMenuEntry(
			ComponentMenuNode&                                root,
			const ComponentRegistry::RegisteredComponentInfo& info
		) {
			if (!IsValidComponentStableNameForMenu(info.stableName)) {
				return false;
			}

			ComponentMenuNode* node    = &root;
			const size_t       lastDot = info.stableName.find_last_of('.');
			if (lastDot != std::string::npos) {
				size_t begin = 0;
				while (begin < lastDot) {
					const size_t end = info.stableName.find('.', begin);
					if (end == std::string::npos || end > lastDot) {
						break;
					}

					const size_t len = end - begin;
					if (len > 0) {
						node = &node->children[info.stableName.substr(
							begin, len
						)];
					}
					begin = end + 1;
				}
			}

			node->components.emplace_back(info);
			return true;
		}

		void SortComponentMenuTree(ComponentMenuNode& node) {
			std::ranges::sort(
				node.components,
				[](
				const ComponentRegistry::RegisteredComponentInfo& lhs,
				const ComponentRegistry::RegisteredComponentInfo& rhs
			) {
					const std::string_view lhsLabel =
						lhs.displayName.empty() ?
							lhs.stableName :
							lhs.displayName;
					const std::string_view rhsLabel =
						rhs.displayName.empty() ?
							rhs.stableName :
							rhs.displayName;
					return lhsLabel < rhsLabel;
				}
			);

			for (auto& child : node.children | std::views::values) {
				SortComponentMenuTree(child);
			}
		}

		bool DrawComponentAddMenuRecursive(
			const ComponentMenuNode&               node,
			Entity&                                entity,
			const std::unordered_set<std::string>& existingStableNames
		) {
			static std::unordered_map<std::string, uint32_t>
				sComponentIconCache;
			bool added = false;

			for (const auto& [scopeName, childNode] : node.children) {
				if (scopeName.empty()) {
					continue;
				}

				if (ImGuiWidgets::BeginMenuEx(
					scopeName.c_str(),
					StrUtil::ConvertToUtf8(kIconBomb).c_str(), true, 4.0f
				)) {
					added |= DrawComponentAddMenuRecursive(
						childNode, entity, existingStableNames
					);
					ImGui::EndMenu();
				}
			}

			for (const auto& info : node.components) {
				if (!IsValidComponentStableNameForMenu(info.stableName)) {
					continue;
				}

				const std::string visibleLabel =
					info.displayName.empty() ?
						info.stableName :
						info.displayName;

				const std::string label =
					visibleLabel + "###" + info.stableName;

				const bool alreadyExists =
					existingStableNames.contains(info.stableName);
				uint32_t componentIcon = kIconQuestionMark;
				if (const auto cached = sComponentIconCache.find(
						info.stableName
					);
					cached != sComponentIconCache.end()) {
					componentIcon = cached->second;
				} else {
					if (const auto component = ComponentRegistry::Get().Create(
						info.stableName
					)) {
						componentIcon = component->GetIcon();
					}
					sComponentIconCache.emplace(info.stableName, componentIcon);
				}

				if (
					ImGuiWidgets::MenuItemWithIcon(
						label.c_str(), componentIcon, nullptr, false,
						!alreadyExists
					)
				) {
					auto component = ComponentRegistry::Get().Create(
						info.stableName
					);
					if (!component) {
						continue;
					}
					entity.AddComponentInstance(std::move(component));
					added = true;
				}
			}

			return added;
		}

		OutlinerFolderNode* EnsureFolderNode(
			OutlinerFolderNode& root, const std::string_view folderPath
		) {
			OutlinerFolderNode* node = &root;
			for (const auto& part : SceneFolderPath::Split(folderPath)) {
				node = &node->children[part];
			}
			return node;
		}

		void BuildOutlinerTree(
			OutlinerFolderNode& root, const Scene& scene
		) {
			for (const std::string& folder : scene.GetFolders()) {
				EnsureFolderNode(root, folder);
			}
			for (const auto& ePtr : scene.GetEntities()) {
				if (!ePtr) {
					continue;
				}
				Entity*             entity = ePtr.get();
				OutlinerFolderNode* node   = EnsureFolderNode(
					root, entity->GetFolderPath()
				);
				node->entities.emplace_back(entity);
			}
		}

		std::string MakeUniqueFolderPath(
			const Scene& scene, const std::string_view parentFolderPath
		) {
			const std::string parent = SceneFolderPath::Normalize(
				parentFolderPath
			);
			int suffix = 0;
			while (true) {
				const std::string candidateName =
					suffix == 0 ?
						"NewFolder" :
						"NewFolder" + std::to_string(suffix);
				const std::string candidatePath = SceneFolderPath::Join(
					parent, candidateName
				);
				if (
					std::ranges::find(scene.GetFolders(), candidatePath) ==
					scene.GetFolders().end()
				) {
					return candidatePath;
				}
				++suffix;
			}
		}

		bool IsTransformAncestor(
			const TransformComponent* possibleAncestor,
			const TransformComponent* node
		) {
			const TransformComponent* current = node;
			while (current) {
				if (current == possibleAncestor) {
					return true;
				}
				current = current->GetParent();
			}
			return false;
		}

		[[nodiscard]] bool IsEntityNameUsedInFolder(
			const Scene&            scene,
			const std::string_view& folderPath,
			const std::string_view& entityName
		) {
			for (const auto& entityPtr : scene.GetEntities()) {
				if (!entityPtr) {
					continue;
				}

				if (
					entityPtr->GetFolderPath() == folderPath &&
					entityPtr->GetName() == entityName
				) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] std::string BuildDuplicateEntityName(
			const Scene&  scene,
			const Entity& source
		) {
			const std::string baseName =
				std::string(source.GetName()) + " Copy";
			if (
				!IsEntityNameUsedInFolder(
					scene, source.GetFolderPath(), baseName
				)
			) {
				return baseName;
			}

			for (uint32_t i = 2; i < 100000; ++i) {
				const std::string candidate =
					std::format("{} {}", baseName, i);
				if (
					!IsEntityNameUsedInFolder(
						scene, source.GetFolderPath(), candidate
					)
				) {
					return candidate;
				}
			}

			return baseName;
		}

		[[nodiscard]] bool IsComponentGuidUsed(
			const Scene&   scene,
			const uint64_t guid
		) {
			if (guid == 0) {
				return true;
			}

			for (const auto& entityPtr : scene.GetEntities()) {
				if (!entityPtr) {
					continue;
				}

				bool found = false;
				entityPtr->ForEachComponent(
					[&](const BaseComponent& component) {
						if (component.GetGuid() != guid) {
							return true;
						}
						found = true;
						return false;
					}
				);
				if (found) {
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] uint64_t AllocateUniqueComponentGuid(const Scene& scene) {
			static GuidGenerator guidGenerator;
			for (uint32_t attempts = 0; attempts < 100000; ++attempts) {
				const uint64_t candidate = guidGenerator.Alloc();
				if (!IsComponentGuidUsed(scene, candidate)) {
					return candidate;
				}
			}
			return 0;
		}

		[[nodiscard]] Entity* DuplicateEntityInScene(
			Scene&        scene,
			const Entity& source
		) {
			const std::string duplicatedName = BuildDuplicateEntityName(
				scene, source
			);
			Entity& duplicated = scene.CreateEntity(
				duplicatedName, scene.AllocateEntityId(), source.IsEditorOnly()
			);
			duplicated.SetFolderPath(source.GetFolderPath());
			duplicated.SetActive(source.IsActive());
			duplicated.SetVisible(source.IsVisible());
			scene.AddFolder(source.GetFolderPath());

			source.ForEachComponent(
				[&](const BaseComponent& sourceComponent) {
					auto newComponent = ComponentRegistry::Get().Create(
						sourceComponent.GetStableName()
					);
					if (!newComponent) {
						Warning(
							"LevelEditorTool",
							"DuplicateEntity: unknown component type '{}'",
							sourceComponent.GetStableName()
						);
						return;
					}

					JsonWriter componentWriter("__duplicate_component__.json");
					componentWriter.BeginObject();
					sourceComponent.Serialize(componentWriter);
					componentWriter.EndObject();
					const JsonReader componentReader(componentWriter.GetRoot());
					newComponent->Deserialize(componentReader);
					newComponent->SetActive(sourceComponent.IsActive());

					const uint64_t duplicatedComponentGuid =
						AllocateUniqueComponentGuid(scene);
					if (duplicatedComponentGuid == 0) {
						Warning(
							"LevelEditorTool",
							"DuplicateEntity: failed to allocate component GUID for '{}'",
							sourceComponent.GetStableName()
						);
					} else {
						newComponent->SetGuid(duplicatedComponentGuid);
					}

					duplicated.AddComponentInstance(std::move(newComponent));
				}
			);

			const auto* sourceTransform = source.GetComponent<
				TransformComponent>();
			auto* duplicatedTransform = duplicated.GetComponent<
				TransformComponent>();
			if (sourceTransform && duplicatedTransform && sourceTransform->
			    GetParent()) {
				duplicatedTransform->SetParent(sourceTransform->GetParent(),
				                               false);
			}

			return &duplicated;
		}
	}

	void LevelEditorTool::DrawSceneOutliner() {
		Scene* scene = GetOutlinerScene();
		if (!scene) {
			return;
		}

		if (!ImGui::Begin("Outliner")) {
			ImGui::End();
			return;
		}

		static uint64_t              renameEntityId = 0;
		static std::string           renameFolderPath;
		static std::array<char, 256> renameBuffer             = {};
		bool                         openRenamePopupRequested = false;

		auto openRenameEntity = [&](const Entity& entity) {
			renameEntityId = entity.GetGuid();
			renameFolderPath.clear();
			std::ranges::fill(renameBuffer, '\0');
			const std::string name(entity.GetName());
			memcpy(
				renameBuffer.data(),
				name.c_str(),
				std::min(name.size(), renameBuffer.size() - 1)
			);
			openRenamePopupRequested = true;
		};

		auto openRenameFolder = [&](const std::string_view folderPath) {
			renameEntityId   = 0;
			renameFolderPath = std::string(folderPath);
			std::ranges::fill(renameBuffer, '\0');
			const std::string leafName = SceneFolderPath::LeafName(folderPath);
			memcpy(
				renameBuffer.data(),
				leafName.c_str(),
				std::min(leafName.size(), renameBuffer.size() - 1)
			);
			openRenamePopupRequested = true;
		};

		constexpr ImGuiTableFlags tableFlags =
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_NoBordersInBodyUntilResize;

		auto createEntity = [&](const std::string_view folderPath) {
			Entity& entity = scene->CreateEntity(
				"Entity", scene->AllocateEntityId(), false
			);
			entity.SetFolderPath(folderPath);
			scene->AddFolder(folderPath);
			entity.SetVisible(true);
			[[maybe_unused]] auto* addedTransform =
				entity.AddComponent<TransformComponent>();
			mSelectedEntityId = entity.GetGuid();
		};
		auto createFolder = [&](const std::string_view folderPath) {
			scene->AddFolder(MakeUniqueFolderPath(*scene, folderPath));
		};

		if (ImGui::BeginPopupContextWindow(
			"OutlinerWindowContext",
			ImGuiPopupFlags_MouseButtonRight |
			ImGuiPopupFlags_NoOpenOverItems
		)) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		float iconScale = 1.5f;

		auto addButtonSize = ImVec2(
			ImGui::GetFontSize() * iconScale,
			ImGui::GetFontSize() * iconScale
		);

		if (
			ImGuiWidgets::IconButton(
				kIconAdd,
				nullptr,
				addButtonSize,
				iconScale
			)
		) {
			ImGui::OpenPopup("OutlinerAddPopup");
		}
		if (ImGui::BeginPopup("OutlinerAddPopup")) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginTable("OutlinerTable", 3, tableFlags)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn(
				"Visible", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableSetupColumn(
				"Active", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableHeadersRow();

			OutlinerFolderNode root = {};
			BuildOutlinerTree(root, *scene);
			uint64_t    pendingDeleteEntityId    = 0;
			uint64_t    pendingDuplicateEntityId = 0;
			std::string pendingDeleteFolderPath;
			bool        pendingCreateEntity = false;
			bool        pendingCreateFolder = false;
			std::string pendingCreateFolderPath;
			uint64_t    pendingParentChildEntityId  = 0;
			uint64_t    pendingParentTargetEntityId = 0;
			uint64_t    pendingMoveEntityId         = 0;
			std::string pendingMoveEntityFolderPath;
			std::string pendingMoveFolderSourcePath;
			std::string pendingMoveFolderTargetPath;
			std::unordered_map<uint64_t, std::vector<Entity*>>
				childEntitiesByParent;
			for (const auto& entityPtr : scene->GetEntities()) {
				if (!entityPtr) {
					continue;
				}
				Entity*     entity    = entityPtr.get();
				const auto* transform = entity->GetComponent<
					TransformComponent>();
				if (
					!transform ||
					!transform->GetParent() ||
					!transform->GetParent()->GetOwner()
				) {
					continue;
				}
				childEntitiesByParent[
					transform->GetParent()->GetOwner()->GetGuid()
				].emplace_back(entity);
			}

			std::function<void(Entity*)> drawEntityNode;
			drawEntityNode = [&](Entity* entity) {
				if (!entity) {
					return;
				}
				std::vector<Entity*> childrenInSameFolder;
				if (const auto it = childEntitiesByParent.
						find(entity->GetGuid());
					it != childEntitiesByParent.end()) {
					for (Entity* child : it->second) {
						if (
							child &&
							std::string(child->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							childrenInSameFolder.emplace_back(child);
						}
					}
				}

				const bool isSelected = entity->GetGuid() == mSelectedEntityId;
				ImGui::PushID(static_cast<int>(entity->GetGuid()));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				const bool hasChildren = !childrenInSameFolder.empty();
				const ImGuiTreeNodeFlags nodeFlags =
					ImGuiTreeNodeFlags_SpanFullWidth |
					(hasChildren ?
						 ImGuiTreeNodeFlags_DefaultOpen |
						 ImGuiTreeNodeFlags_OpenOnDoubleClick :
						 ImGuiTreeNodeFlags_Leaf |
						 ImGuiTreeNodeFlags_NoTreePushOnOpen) |
					(isSelected ? ImGuiTreeNodeFlags_Selected : 0);
				ImGui::BeginDisabled(!entity->IsActive());
				const bool opened = ImGui::TreeNodeEx(
					reinterpret_cast<void*>(
						entity->GetGuid()
					),
					nodeFlags,
					"%s",
					entity->GetName().data()
				);
				ImGui::EndDisabled();
				if (ImGui::IsItemClicked()) {
					mSelectedEntityId = entity->GetGuid();
				}
				if (ImGui::BeginDragDropSource()) {
					const uint64_t guid = entity->GetGuid();
					ImGui::SetDragDropPayload(
						"OUTLINER_ENTITY", &guid, sizeof(guid)
					);
					ImGui::TextUnformatted(entity->GetName().data());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_ENTITY"
						)) {
						const uint64_t draggedGuid = *static_cast<const uint64_t
							*>(
							payload->Data
						);
						if (draggedGuid != entity->GetGuid()) {
							pendingParentChildEntityId  = draggedGuid;
							pendingParentTargetEntityId = entity->GetGuid();
						}
					}
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_FOLDER"
						)) {
						const auto sourcePath = static_cast<const char*>(
							payload->Data
						);
						if (sourcePath) {
							pendingMoveFolderSourcePath = sourcePath;
							pendingMoveFolderTargetPath = std::string(
								entity->GetFolderPath()
							);
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::BeginPopupContextItem("EntityContext")) {
					if (ImGui::MenuItem("Add Entity")) {
						pendingCreateEntity     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Duplicate")) {
						pendingDuplicateEntityId = entity->GetGuid();
					}
					if (ImGui::MenuItem("Add Folder")) {
						pendingCreateFolder     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Rename")) {
						openRenameEntity(*entity);
					}
					if (ImGui::MenuItem("Delete")) {
						pendingDeleteEntityId = entity->GetGuid();
					}
					ImGui::EndPopup();
				}

				const auto fontSize = ImVec2(
					ImGui::GetFontSize(), ImGui::GetFontSize()
				);

				ImGui::TableNextColumn();
				const bool visible = entity->IsVisible();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							visible ? kIconVisibility : kIconVisibilityOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetVisible(!visible);
				}

				ImGui::TableNextColumn();
				const bool active = entity->IsActive();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							active ? kIconCheckBoxOn : kIconCheckBoxOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetActive(!active);
				}

				if (opened && hasChildren) {
					for (Entity* child : childrenInSameFolder) {
						drawEntityNode(child);
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			};

			std::function<void(const OutlinerFolderNode&, const std::string&)>
				drawFolder;
			drawFolder = [&](
				const OutlinerFolderNode& node,
				const std::string&        folderPath
			) {
					if (!folderPath.empty()) {
						const std::string displayName =
							SceneFolderPath::LeafName(
								folderPath
							);
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						const bool opened = ImGui::TreeNodeEx(
							folderPath.c_str(),
							ImGuiTreeNodeFlags_SpanFullWidth |
							ImGuiTreeNodeFlags_DefaultOpen |
							ImGuiTreeNodeFlags_OpenOnDoubleClick,
							"%s",
							displayName.c_str()
						);
						if (ImGui::BeginDragDropSource()) {
							ImGui::SetDragDropPayload(
								"OUTLINER_FOLDER",
								folderPath.c_str(),
								folderPath.size() + 1
							);
							ImGui::TextUnformatted(displayName.c_str());
							ImGui::EndDragDropSource();
						}
						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_ENTITY"
								)) {
								const uint64_t draggedGuid = *static_cast<
									const uint64_t*>(payload->Data);
								pendingMoveEntityId         = draggedGuid;
								pendingMoveEntityFolderPath = folderPath;
							}
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_FOLDER"
								)) {
								const auto sourcePath = static_cast<const char
									*>(
									payload->Data
								);
								if (
									sourcePath &&
									folderPath != sourcePath &&
									!SceneFolderPath::IsEqualOrDescendant(
										folderPath, sourcePath
									)
								) {
									pendingMoveFolderSourcePath = sourcePath;
									pendingMoveFolderTargetPath = folderPath;
								}
							}
							ImGui::EndDragDropTarget();
						}
						if (ImGui::BeginPopupContextItem("FolderContext")) {
							if (ImGui::MenuItem("Add Entity")) {
								pendingCreateEntity     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Add Folder")) {
								pendingCreateFolder     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Rename")) {
								openRenameFolder(folderPath);
							}
							if (ImGui::MenuItem("Delete")) {
								pendingDeleteFolderPath = folderPath;
							}
							ImGui::EndPopup();
						}
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();
						if (!opened) {
							return;
						}
					}

					for (const auto& [childName, childNode] : node.children) {
						const std::string childPath = folderPath.empty() ?
								childName :
								folderPath + "/" + childName;
						drawFolder(childNode, childPath);
					}

					for (Entity* entity : node.entities) {
						const auto* transform = entity ?
							                        entity->GetComponent<
								                        TransformComponent>() :
							                        nullptr;
						const auto* parentTransform = transform ?
								transform->GetParent() :
								nullptr;
						const Entity* parentEntity = parentTransform ?
								parentTransform->GetOwner() :
								nullptr;
						if (
							parentEntity &&
							std::string(parentEntity->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							continue;
						}
						drawEntityNode(entity);
					}

					if (!folderPath.empty()) {
						ImGui::TreePop();
					}
				};

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TreeNodeEx(
				"__outliner_root__",
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanFullWidth,
				"Root"
			);
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_FOLDER"
				)) {
					auto sourcePath = static_cast<const char*>(payload->
						Data);
					if (sourcePath) {
						pendingMoveFolderSourcePath = sourcePath;
						pendingMoveFolderTargetPath.clear();
					}
				}
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_ENTITY"
				)) {
					const uint64_t draggedGuid = *static_cast<const uint64_t*>(
						payload->Data
					);
					pendingMoveEntityId = draggedGuid;
					pendingMoveEntityFolderPath.clear();
				}
				ImGui::EndDragDropTarget();
			}
			if (ImGui::BeginPopupContextItem("RootContext")) {
				if (ImGui::MenuItem("Add Entity")) {
					pendingCreateEntity = true;
				}
				if (ImGui::MenuItem("Add Folder")) {
					pendingCreateFolder = true;
				}
				ImGui::EndPopup();
			}
			ImGui::TableNextColumn();
			ImGui::TableNextColumn();

			drawFolder(root, "");

			ImGui::EndTable();

			if (pendingCreateEntity) {
				createEntity(pendingCreateFolderPath);
			}
			if (pendingDuplicateEntityId != 0) {
				if (Entity* sourceEntity = scene->FindEntity(
					pendingDuplicateEntityId)) {
					if (Entity* duplicated = DuplicateEntityInScene(
						*scene, *sourceEntity)) {
						mSelectedEntityId = duplicated->GetGuid();
					}
				}
			}
			if (pendingCreateFolder) {
				createFolder(pendingCreateFolderPath);
			}
			if (pendingMoveEntityId != 0) {
				if (Entity* entity = scene->FindEntity(pendingMoveEntityId)) {
					entity->SetFolderPath(pendingMoveEntityFolderPath);
					scene->AddFolder(pendingMoveEntityFolderPath);
				}
			}
			if (!pendingMoveFolderSourcePath.empty()) {
				scene->MoveFolderSubtree(
					pendingMoveFolderSourcePath, pendingMoveFolderTargetPath
				);
			}
			if (
				pendingParentChildEntityId != 0 &&
				pendingParentTargetEntityId != 0
			) {
				Entity* childEntity = scene->FindEntity(
					pendingParentChildEntityId
				);
				Entity* parentEntity = scene->FindEntity(
					pendingParentTargetEntityId
				);
				if (childEntity && parentEntity) {
					auto* childTransform = childEntity->GetComponent<
						TransformComponent>();
					auto* parentTransform = parentEntity->GetComponent<
						TransformComponent>();
					if (
						childTransform &&
						parentTransform &&
						childTransform != parentTransform &&
						!IsTransformAncestor(childTransform, parentTransform)
					) {
						childTransform->SetParent(parentTransform);
						childEntity->SetFolderPath(
							parentEntity->GetFolderPath()
						);
						scene->AddFolder(parentEntity->GetFolderPath());
					}
				}
			}
			if (pendingDeleteEntityId != 0) {
				if (mSelectedEntityId == pendingDeleteEntityId) {
					mSelectedEntityId = 0;
				}
				scene->DestroyEntity(pendingDeleteEntityId);
			}
			if (!pendingDeleteFolderPath.empty()) {
				scene->DeleteFolderSubtree(pendingDeleteFolderPath);
			}
		}

		if (openRenamePopupRequested) {
			ImGui::OpenPopup("OutlinerRenamePopup");
			openRenamePopupRequested = false;
		}

		if (ImGui::BeginPopup("OutlinerRenamePopup")) {
			ImGui::InputText(
				"##Rename", renameBuffer.data(), renameBuffer.size()
			);
			if (ImGui::Button("Apply")) {
				if (renameEntityId != 0) {
					if (Entity* entity = scene->FindEntity(renameEntityId)) {
						entity->SetName(renameBuffer.data());
					}
				} else if (!renameFolderPath.empty()) {
					scene->RenameFolderSubtree(
						renameFolderPath, renameBuffer.data()
					);
				}
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void LevelEditorTool::DrawInspector() const {
		if (!ImGui::Begin("Inspector")) {
			ImGui::End();
			return;
		}

		Entity* entity = GetSelectedEntity();
		if (!entity) {
			ImGui::TextUnformatted("No selected entity.");
			ImGui::End();
			return;
		}

		std::unordered_set<std::string> existingStableNames;
		entity->ForEachComponent(
			[&](const BaseComponent& component) {
				existingStableNames.emplace(component.GetStableName());
			}
		);

		ImDrawList* dl       = ImGui::GetWindowDrawList();
		ImFont*     font     = ImGui::GetFont();
		const float textSize = ImGui::GetFontSize() * 1.5f;

		// アイコンを描画
		dl->AddText(
			font, textSize, ImGui::GetCursorScreenPos(),
			ImGui::GetColorU32(ImGuiCol_Text),
			entity->GetName().data()
		);

		// テキストの下にスペースを空ける
		ImGui::SetCursorPosY(
			ImGui::GetCursorPosY() + textSize + ImGui::GetStyle().ItemSpacing.y
		);

		const std::string label = "GUID: " + std::to_string(entity->GetGuid());
		if (
			ImGuiWidgets::IconButton(
				kIconCopy, label.c_str(), ImVec2(0, 0), 1.0f, ImGuiDir_Right
			)
		) {
			ImGui::SetClipboardText(std::to_string(entity->GetGuid()).c_str());
		}

		if (
			ImGuiWidgets::IconButton(
				kIconAdd, "AddComponent", ImVec2(0.0f, 0.0f), 1.5f,
				ImGuiDir_Right
			)
		) {
			ImGui::OpenPopup("InspectorAddComponentPopup");
		}

		ImGuiWidgets::BeginMenu();

		if (ImGui::BeginPopup("InspectorAddComponentPopup")) {
			ComponentMenuNode addComponentRoot    = {};
			size_t            validComponentCount = 0;

			for (
				const auto& info :
				ComponentRegistry::Get().ListRegisteredComponents()
			) {
				if (InsertComponentMenuEntry(addComponentRoot, info)) {
					++validComponentCount;
				}
			}
			SortComponentMenuTree(addComponentRoot);

			if (validComponentCount == 0) {
				// なかなかないけど一応
				ImGui::TextUnformatted("コンポーネントが登録されていません。");
			} else if (
				DrawComponentAddMenuRecursive(
					addComponentRoot, *entity, existingStableNames
				)
			) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGuiWidgets::EndMenu();

		ImGui::Separator();

		std::vector<BaseComponent*> orderedComponents;
		entity->ForEachComponent(
			[&](BaseComponent& component) {
				orderedComponents.emplace_back(&component);
			}
		);

		auto pendingAction =
			ImGuiWidgets::HeaderMenuAction::None;
		BaseComponent* pendingTarget = nullptr;

		for (size_t index = 0; index < orderedComponents.size(); ++index) {
			BaseComponent* component = orderedComponents[index];
			if (!component) {
				continue;
			}

			const bool isTransform =
				component->GetStableName() == "Transform";
			const bool canMoveUp =
				!isTransform &&
				index > 0 &&
				orderedComponents[index - 1] != nullptr &&
				orderedComponents[index - 1]->GetStableName() != "Transform";
			const bool canMoveDown =
				!isTransform && index + 1 < orderedComponents.size();
			const bool canRemove = !isTransform;

			bool componentActive = component->IsActive();
			auto action          =
				ImGuiWidgets::HeaderMenuAction::None;
			const bool open = ImGuiWidgets::CollapsingHeaderWithCheckbox(
				component->GetIcon(),
				component->GetComponentName().data(),
				component->GetGuid(),
				&componentActive,
				&action,
				canMoveUp,
				canMoveDown,
				canRemove,
				ImGuiTreeNodeFlags_DefaultOpen
			);
			if (componentActive != component->IsActive()) {
				component->SetActive(componentActive);
			}
			if (open) {
				component->DrawInspectorImGui();
			}
			ImGui::Separator();

			if (
				action != ImGuiWidgets::HeaderMenuAction::None &&
				pendingTarget == nullptr
			) {
				pendingAction = action;
				pendingTarget = component;
			}
		}

		if (pendingTarget != nullptr) {
			switch (pendingAction) {
				case ImGuiWidgets::HeaderMenuAction::MoveUp: (void)entity->
						MoveComponentUp(pendingTarget);
					break;
				case ImGuiWidgets::HeaderMenuAction::MoveDown: (void)entity->
						MoveComponentDown(pendingTarget);
					break;
				case ImGuiWidgets::HeaderMenuAction::Remove: entity->
						RemoveComponent(pendingTarget);
					break;
				case ImGuiWidgets::HeaderMenuAction::None:
				default: break;
			}
		}

		ImGui::End();
	}

	void LevelEditorTool::DrawContentBrowser() {
		EditorContentBrowser::DrawWindow(
			mContentBrowserState,
			"Content Browser",
			[this](const std::string& path, const ASSET_TYPE type) {
				if (
					type != ASSET_TYPE::SEQUENCE ||
					!mSequenceEditorController
				) {
					return;
				}
				(void)mSequenceEditorController->OpenDocument(Path(path));
			}
		);
	}
}

#endif
