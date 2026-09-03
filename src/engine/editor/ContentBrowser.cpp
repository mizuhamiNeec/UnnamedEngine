#ifdef _DEBUG
#include "ContentBrowser.h"
#include "core/filesystem/Path.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <format>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "core/assets/AssetManager.h"
#include "core/content/ContentPathResolver.h"

#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/content/ContentMountDefinitions.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::EditorContentBrowser {
	namespace {
		namespace fs = std::filesystem;

		/// @brief BrowserEntryは、Content Browserに表示するpath、表示名、directory・asset種別を保持します
		struct BrowserEntry {
			Path        path;
			std::string virtualPath;
			std::string name;
			bool        isDirectory = false;
			ASSET_TYPE  type        = ASSET_TYPE::UNKNOWN;
		};

		std::unordered_map<ImGuiID, BrowserViewState> sPickerStates;

		const ContentPathResolver* GetContentResolver() {
			if (const auto* assetManager = ServiceLocator::Get<
				AssetManager>()) {
				return &assetManager->GetContentPathResolver();
			}
			return nullptr;
		}

		bool SetActiveMount(
			BrowserViewState& state, const std::string_view mountId
		) {
			const ContentPathResolver* resolver = GetContentResolver();
			if (!resolver) {
				return false;
			}
			for (const ContentDirectoryMount& mount : resolver->GetMounts()) {
				if (mount.id != mountId) {
					continue;
				}
				state.selectedMountId = mount.id;
				state.rootPath        = mount.rootPath.LexicallyNormal();
				state.currentPath     = state.rootPath;
				state.selectedPath.Clear();
				return true;
			}
			return false;
		}

		bool EnsureActiveMount(BrowserViewState& state) {
			const ContentPathResolver* resolver = GetContentResolver();
			if (!resolver || resolver->GetMounts().empty()) {
				return false;
			}
			if (!state.selectedMountId.empty()) {
				for (const ContentDirectoryMount& mount : resolver->
				     GetMounts()) {
					if (mount.id != state.selectedMountId) {
						continue;
					}
					if (state.rootPath.LexicallyNormal() ==
					    mount.rootPath.LexicallyNormal()) {
						return true;
					}
					return SetActiveMount(state, state.selectedMountId);
				}
			}
			// New assets are game-owned by default.  Fall back to the highest
			// priority mount when the application has no game content.
			if (SetActiveMount(state, ContentMountId::kGame)) {
				return true;
			}
			return SetActiveMount(state, resolver->GetMounts().front().id);
		}

		std::optional<ResolvedContentFile>
		DescribeMountedPath(const Path& path) {
			const ContentPathResolver* resolver = GetContentResolver();
			if (!resolver) {
				return std::nullopt;
			}
			const auto mountId = resolver->FindMountIdForResolvedPath(path);
			return mountId ?
				       resolver->DescribePathFromMount(*mountId, path) :
				       std::nullopt;
		}

		bool IsPathInsideRoot(const Path& path, const Path& root) {
			// 文字列の接頭辞だけで sibling directory を root 内と誤認しない
			const std::string normalizedPath =
				path.LexicallyNormal().ToGenericUtf8();
			const std::string normalizedRoot =
				root.LexicallyNormal().ToGenericUtf8();
			if (normalizedPath == normalizedRoot) {
				return true;
			}
			if (normalizedPath.size() <= normalizedRoot.size()) {
				return false;
			}
			return normalizedPath.starts_with(normalizedRoot) &&
			       normalizedPath[normalizedRoot.size()] == '/';
		}

		bool TryCommitAssetPath(
			std::string&        ioPath,
			const Path&         candidate,
			const AssetTypeMask acceptedMask
		) {
			const Path normalized = candidate.LexicallyNormal();
			if (normalized.IsEmpty()) {
				ioPath.clear();
				return true;
			}

			const ASSET_TYPE guessedType = GuessAssetTypeFromPath(normalized);
			if (!IsAssetTypeAccepted(guessedType, acceptedMask)) {
				return false;
			}

			auto* assetManager = ServiceLocator::Get<AssetManager>();
			if (!assetManager) {
				return false;
			}
			const ContentPathResolver& resolver =
				assetManager->GetContentPathResolver();
			std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(normalized.ToGenericUtf8());
			if (!virtualPath.has_value()) {
				const auto described = DescribeMountedPath(normalized);
				if (!described.has_value()) {
					return false;
				}
				virtualPath = described->virtualPath;
			}
			if (!resolver.ResolveFile(*virtualPath).has_value()) {
				return false;
			}
			if (guessedType != ASSET_TYPE::UNKNOWN) {
				(void)assetManager->LoadAsset(*virtualPath, guessedType);
			}
			ioPath = virtualPath->String();
			return true;
		}

		void EnumerateDirectories(
			const Path& currentDir, std::vector<Path>& outDirs
		) {
			outDirs.clear();
			std::error_code ec;
			for (fs::directory_iterator it(currentDir.Native(), ec), end;
			     !ec && it != end;
			     it.increment(ec)) {
				if (ec) {
					break;
				}
				if (!it->is_directory(ec) || ec) {
					continue;
				}
				outDirs.emplace_back(Path::FromNative(it->path()));
			}
			// 表示順をファイルシステム列挙順に依存させない
			std::ranges::sort(
				outDirs,
				[](const Path& lhs, const Path& rhs) {
					return lhs.FileName().Native() < rhs.FileName().Native();
				}
			);
		}

		void EnumerateEntries(
			const fs::path&            currentDir,
			const AssetTypeMask        acceptedMask,
			std::vector<BrowserEntry>& outEntries
		) {
			outEntries.clear();
			std::error_code ec;
			for (fs::directory_iterator it(currentDir, ec), end;
			     !ec && it != end;
			     it.increment(ec)) {
				if (ec) {
					break;
				}

				const fs::directory_entry& de = *it;
				BrowserEntry               entry = {};
				entry.path = Path::FromNative(de.path()).LexicallyNormal();
				if (const auto described = DescribeMountedPath(entry.path)) {
					entry.virtualPath = described->virtualPath.String();
				}
				entry.name        = Path::ToUtf8String(entry.path.FileName());
				entry.isDirectory = de.is_directory(ec);
				if (ec) {
					continue;
				}

				if (!entry.isDirectory) {
					entry.type = GuessAssetTypeFromPath(entry.path);
					if (!IsAssetTypeAccepted(entry.type, acceptedMask)) {
						continue;
					}
				}
				outEntries.emplace_back(std::move(entry));
			}

			std::ranges::sort(
				outEntries,
				[](const BrowserEntry& lhs, const BrowserEntry& rhs) {
					if (lhs.isDirectory != rhs.isDirectory) {
						return lhs.isDirectory > rhs.isDirectory;
					}
					return lhs.name < rhs.name;
				}
			);
		}

		void EmitAssetPayload(const BrowserEntry& entry) {
			if (entry.isDirectory) {
				return;
			}
			if (!ImGui::BeginDragDropSource()) {
				return;
			}

			AssetDragDropPayload payload = {};
			payload.assetType = static_cast<uint16_t>(entry.type);
			const std::string& normalizedPath = entry.virtualPath;
			if (normalizedPath.empty()) {
				ImGui::EndDragDropSource();
				return;
			}
			memcpy(
				payload.path,
				normalizedPath.c_str(),
				std::min(normalizedPath.size(), sizeof(payload.path) - 1)
			);
			ImGui::SetDragDropPayload(
				kAssetDragDropPayloadId,
				&payload,
				sizeof(payload)
			);
			ImGui::TextUnformatted(entry.name.c_str());
			ImGui::EndDragDropSource();
		}

		std::string MakeUniqueAssetName(
			const Path&            directory, const std::string_view stem,
			const std::string_view extension
		) {
			for (uint32_t suffix = 1; suffix < 10000; ++suffix) {
				const std::string name = suffix == 1 ?
					                         std::string(stem) + std::string(
						                         extension
					                         ) :
					                         std::format(
						                         "{}{}{}",
						                         stem,
						                         suffix,
						                         extension
					                         );
				if (!(directory / Path(name)).IsRegularFile()) {
					return name;
				}
			}
			return {};
		}

		bool CreateAssetTemplate(
			BrowserViewState&      state, const std::string_view     stem,
			const std::string_view extension, const std::string_view text
		) {
			if (state.selectedMountId != ContentMountId::kGame) {
				return false;
			}
			const std::string name = MakeUniqueAssetName(
				state.currentPath,
				stem,
				extension
			);
			if (name.empty()) {
				return false;
			}
			std::error_code ec;
			fs::create_directories(state.currentPath.Native(), ec);
			if (ec) {
				return false;
			}
			const Path filePath = (state.currentPath / Path(name)).
				LexicallyNormal();
			std::ofstream stream(
				filePath.Native(),
				std::ios::binary | std::ios::trunc
			);
			if (!stream) {
				return false;
			}
			stream << text;
			stream.close();
			if (!stream) {
				return false;
			}
			state.selectedPath = filePath;
			return true;
		}

		void DrawCreateAssetContextMenu(BrowserViewState& state) {
			if (!ImGui::BeginPopupContextWindow(
				"ContentBrowserCreateAsset",
				ImGuiPopupFlags_MouseButtonRight
			)) {
				return;
			}

			const bool canCreate =
				state.selectedMountId == ContentMountId::kGame;
			if (!canCreate) {
				ImGui::BeginDisabled();
			}
			if (ImGui::MenuItem("New Empty Scene")) {
				(void)CreateAssetTemplate(
					state,
					"NewScene",
					".scene.json",
					"{\n  \"folders\": [],\n  \"entities\": []\n}\n"
				);
			}
			if (ImGui::MenuItem("New Material")) {
				(void)CreateAssetTemplate(
					state,
					"NewMaterial",
					".material.json",
					"{\n  \"name\": \"NewMaterial\",\n  \"shader\": \"shaders/programs/pbr.shader.json\",\n  \"domain\": \"pbr\",\n  \"shadingModel\": \"LitPBR\",\n  \"renderState\": { \"depthEnable\": true, \"depthWrite\": true, \"cullBackFace\": true, \"blendEnable\": false, \"castsShadow\": true },\n  \"scalars\": {},\n  \"vectors\": {}\n}\n"
				);
			}
			if (ImGui::MenuItem("New UI Document")) {
				(void)CreateAssetTemplate(
					state,
					"NewUiDocument",
					".ui.json",
					"{\n  \"version\": 2,\n  \"name\": \"NewUiDocument\",\n  \"root\": { \"name\": \"Root\", \"visible\": true, \"enabled\": true, \"components\": [], \"children\": [] }\n}\n"
				);
			}
			if (!canCreate) {
				ImGui::EndDisabled();
				ImGui::Separator();
				ImGui::TextDisabled(
					"Core content is read-only. Select Game to create assets."
				);
			}
			ImGui::EndPopup();
		}

		void DrawTreeRecursive(
			const Path& rootPath, const Path& nodePath,
			Path&       currentPath
		) {
			const auto node       = nodePath;
			const auto current    = currentPath.LexicallyNormal();
			const bool isCurrent  = node == current;
			const bool isAncestor = currentPath.ParentPath() == node;

			std::vector<Path> children;
			EnumerateDirectories(nodePath, children);

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_SpanFullWidth |
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				(isCurrent ? ImGuiTreeNodeFlags_Selected : 0);
			if (children.empty()) {
				flags |= ImGuiTreeNodeFlags_Leaf |
					ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (isAncestor) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			}

			const std::string label = nodePath == rootPath ?
				                          "content" :
				                          nodePath.FileName().ToUtf8();
			const bool opened = ImGui::TreeNodeEx(
				node.ToUtf8().c_str(),
				flags,
				"%s %s",
				StrUtil::ConvertToUtf8(kIconGroup).c_str(),
				label.c_str()
			);
			if (ImGui::IsItemClicked()) {
				currentPath = node;
			}

			if (!opened || children.empty()) {
				return;
			}
			for (const Path& child : children) {
				DrawTreeRecursive(rootPath, child, currentPath);
			}
			ImGui::TreePop();
		}

		std::string FitTextWithEllipsis(
			std::string text, const float maxWidth
		) {
			if (text.empty() || maxWidth <= 4.0f) {
				return text;
			}
			if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
				return text;
			}
			static constexpr std::string_view kEllipsis = "...";
			while (
				!text.empty() &&
				ImGui::CalcTextSize((text + std::string(kEllipsis)).c_str()).x >
				maxWidth
			) {
				text.pop_back();
			}
			return text.empty() ?
				       std::string(kEllipsis) :
				       text + std::string(kEllipsis);
		}

		bool DrawIconCell(
			const BrowserEntry& entry,
			const bool          selected,
			const float         iconAreaSize,
			bool&               outDoubleClicked
		) {
			constexpr float padding        = 6.0f;
			const float     textAreaHeight = ImGui::GetTextLineHeight() * 2.0f;
			const float     cellHeight     =
				iconAreaSize + textAreaHeight + padding * 3.0f;
			const ImVec2 cellSize(iconAreaSize, cellHeight);

			const std::string cellId =
				entry.path.LexicallyNormal().ToGenericUtf8();
			ImGui::PushID(cellId.c_str());
			const bool clicked = ImGui::Selectable(
				"##asset_cell",
				selected,
				ImGuiSelectableFlags_AllowDoubleClick,
				cellSize
			);
			outDoubleClicked = clicked && ImGui::IsMouseDoubleClicked(
				                   ImGuiMouseButton_Left
			                   );

			const ImVec2 cellMin   = ImGui::GetItemRectMin();
			const ImVec2 cellMax   = ImGui::GetItemRectMax();
			ImDrawList*  drawList  = ImGui::GetWindowDrawList();
			const ImU32  textColor = ImGui::GetColorU32(ImGuiCol_Text);

			uint32_t icon = kIconQuestionMark;

			if (entry.isDirectory) {
				icon = kIconFolder;
			} else {
				switch (entry.type) {
					case ASSET_TYPE::UNKNOWN: icon = kIconQuestionMark;
						break;
					case ASSET_TYPE::TEXTURE: icon = kIconTexture;
						break;
					case ASSET_TYPE::SHADER_SOURCE: icon = kIconCode;
						break;
					case ASSET_TYPE::MATERIAL: icon = kIconEvShadow;
						break;
					case ASSET_TYPE::MESH: icon = kIconMesh;
						break;
					case ASSET_TYPE::SOUND: icon = kIconAudioFile;
						break;
					case ASSET_TYPE::RAW_FILE: icon = kIconArticle;
						break;
					case ASSET_TYPE::SHADER_PROGRAM: icon = kIconJson;
						break;
					case ASSET_TYPE::MATERIAL_INSTANCE: icon = kIconJson;
						break;
					case ASSET_TYPE::POST_FX_CHAIN: icon = kIconJson;
						break;
					case ASSET_TYPE::UI_DOCUMENT: icon = kIconDesktopLandscape;
						break;
					case ASSET_TYPE::EVENT_PRESENTATION: icon = kIconJson;
						break;
				}
			}

			const std::string iconUtf8     = StrUtil::ConvertToUtf8(icon);
			const float       iconFontSize = std::clamp(
				iconAreaSize * 0.42f,
				18.0f,
				34.0f
			);
			const ImVec2 iconSize = ImGui::CalcTextSize(iconUtf8.c_str());
			const ImVec2 iconPos(
				cellMin.x + (iconAreaSize - iconSize.x) * 0.5f,
				cellMin.y + padding
			);
			drawList->AddText(
				ImGui::GetFont(),
				iconFontSize,
				iconPos,
				textColor,
				iconUtf8.c_str()
			);

			const float       textMaxWidth = iconAreaSize - padding * 2.0f;
			const std::string displayName  = FitTextWithEllipsis(
				entry.name,
				textMaxWidth
			);
			const ImVec2 textPos(
				cellMin.x + padding,
				cellMax.y - textAreaHeight - padding
			);
			drawList->AddText(textPos, textColor, displayName.c_str());

			ImGui::PopID();
			return clicked;
		}

		bool DrawContentView(
			BrowserViewState&        state,
			const AssetTypeMask      acceptedMask,
			const bool               emitDragPayload,
			const bool               allowAssetCreation,
			std::string*             outCommittedPath,
			const AssetOpenCallback* onAssetOpen
		) {
			const Path rootPath    = state.rootPath.LexicallyNormal();
			Path       currentPath = state.currentPath.LexicallyNormal();
			if (!IsPathInsideRoot(currentPath, rootPath)) {
				currentPath = rootPath;
			}

			bool committed = false;

			if (
				ImGui::BeginTable(
					"##ContentBrowserSplit",
					2,
					ImGuiTableFlags_Resizable |
					ImGuiTableFlags_BordersInnerV |
					ImGuiTableFlags_SizingFixedFit,
					ImVec2(0.0f, 420.0f)
				)
			) {
				ImGui::TableSetupColumn(
					"Tree",
					ImGuiTableColumnFlags_WidthFixed,
					260.0f
				);
				ImGui::TableSetupColumn(
					"Main",
					ImGuiTableColumnFlags_WidthStretch,
					0.0f
				);

				ImGui::TableNextColumn();
				ImGui::BeginChild(
					"##ContentBrowserTree",
					ImVec2(0.0f, 0.0f),
					false
				);
				DrawTreeRecursive(rootPath, rootPath, currentPath);
				currentPath = currentPath.LexicallyNormal();
				ImGui::EndChild();

				ImGui::TableNextColumn();
				ImGui::BeginChild(
					"##ContentBrowserMain",
					ImVec2(0.0f, 0.0f),
					false
				);

				std::vector<BrowserEntry> entries;
				EnumerateEntries(currentPath.Native(), acceptedMask, entries);
				if (!state.iconView) {
					for (const BrowserEntry& entry : entries) {
						const auto normalizedPath =
							entry.path.LexicallyNormal();
						const std::string row = std::format(
							"{} {}###{}",
							entry.isDirectory ?
								StrUtil::ConvertToUtf8(kIconGroup) :
								StrUtil::ConvertToUtf8(kIconObject),
							entry.name,
							normalizedPath
						);
						const bool selected =
							state.selectedPath == normalizedPath;
						if (
							ImGui::Selectable(
								row.c_str(),
								selected,
								ImGuiSelectableFlags_AllowDoubleClick
							)
						) {
							state.selectedPath = normalizedPath;
							if (
								entry.isDirectory &&
								ImGui::IsMouseDoubleClicked(
									ImGuiMouseButton_Left
								)
							) {
								currentPath = entry.path.LexicallyNormal();
							} else if (
								!entry.isDirectory &&
								ImGui::IsMouseDoubleClicked(
									ImGuiMouseButton_Left
								)
							) {
								if (outCommittedPath) {
									*outCommittedPath = entry.virtualPath;
									committed         = true;
								} else if (onAssetOpen && *onAssetOpen) {
									(*onAssetOpen)(
										entry.virtualPath,
										entry.type
									);
								}
							}
						}
						if (emitDragPayload) {
							EmitAssetPayload(entry);
						}
					}
				} else {
					const ImGuiIO& io = ImGui::GetIO();
					if (
						ImGui::IsWindowHovered() &&
						io.KeyCtrl &&
						io.MouseWheel != 0.0f
					) {
						state.iconSize = std::clamp(
							state.iconSize + io.MouseWheel * 8.0f,
							56.0f,
							220.0f
						);
					}

					const float spacing   = ImGui::GetStyle().ItemSpacing.x;
					const float cellWidth = std::max(56.0f, state.iconSize);
					const int   columns   = std::max(
						1,
						static_cast<int>(
							(ImGui::GetContentRegionAvail().x + spacing) /
							(cellWidth + spacing)
						)
					);

					int column = 0;
					for (const BrowserEntry& entry : entries) {
						const auto normalizedPath = entry.path.
							LexicallyNormal();
						bool       doubleClicked = false;
						const bool clicked       = DrawIconCell(
							entry,
							state.selectedPath == normalizedPath,
							cellWidth,
							doubleClicked
						);
						if (clicked) {
							state.selectedPath = normalizedPath;
							if (entry.isDirectory && doubleClicked) {
								currentPath = entry.path.LexicallyNormal();
							} else if (
								!entry.isDirectory && doubleClicked
							) {
								if (outCommittedPath) {
									*outCommittedPath = entry.virtualPath;
									committed         = true;
								} else if (onAssetOpen && *onAssetOpen) {
									(*onAssetOpen)(
										entry.virtualPath,
										entry.type
									);
								}
							}
						}
						if (emitDragPayload) {
							EmitAssetPayload(entry);
						}

						++column;
						if (column < columns) {
							ImGui::SameLine();
						} else {
							column = 0;
						}
					}
				}

				if (allowAssetCreation) {
					DrawCreateAssetContextMenu(state);
				}
				ImGui::EndChild();
				ImGui::EndTable();
			}

			state.currentPath = currentPath.LexicallyNormal();
			return committed;
		}

		bool DrawTopBar(BrowserViewState& state) {
			if (!EnsureActiveMount(state)) {
				ImGui::TextUnformatted("No content mounts are available.");
				return false;
			}
			const Path rootPath    = state.rootPath.LexicallyNormal();
			Path       currentPath = state.currentPath.LexicallyNormal();
			if (!IsPathInsideRoot(currentPath, rootPath)) {
				currentPath = rootPath;
			}

			const ContentPathResolver* resolver = GetContentResolver();
			if (resolver && ImGui::BeginCombo(
				    "Mount",
				    state.selectedMountId.c_str()
			    )) {
				for (const ContentDirectoryMount& mount : resolver->
				     GetMounts()) {
					const bool selected = state.selectedMountId == mount.id;
					if (ImGui::Selectable(mount.id.c_str(), selected)) {
						(void)SetActiveMount(state, mount.id);
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconArrowUpward,
					nullptr,
					ImVec2(0, 0),
					1.0f
				)
			) {
				const Path parent = currentPath.ParentPath();
				if (IsPathInsideRoot(parent, rootPath)) {
					currentPath = parent;
				}
			}
			ImGui::SameLine();
			int viewMode = state.iconView ? 1 : 0;
			ImGui::RadioButton("List", &viewMode, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Icon", &viewMode, 1);
			state.iconView = viewMode == 1;
			ImGui::SameLine();
			const auto currentDescription = DescribeMountedPath(currentPath);
			const std::string currentPathText = currentDescription ?
				                                    currentDescription->
				                                    virtualPath.String() :
				                                    currentPath.
				                                    LexicallyNormal().
				                                    ToGenericUtf8();
			ImGui::TextDisabled("%s", currentPathText.c_str());

			state.currentPath = currentPath.LexicallyNormal();
			return true;
		}
	}

	AssetTypeMask AssetTypeToMask(const ASSET_TYPE type) {
		return static_cast<AssetTypeMask>(type);
	}

	bool IsAssetTypeAccepted(
		const ASSET_TYPE    type,
		const AssetTypeMask acceptedMask
	) {
		if (acceptedMask == kAssetTypeMaskAny) {
			return true;
		}
		const AssetTypeMask typeMask = static_cast<AssetTypeMask>(type);
		return (acceptedMask & typeMask) != 0;
	}

	bool DrawAssetPathPicker(
		const char*         label,
		std::string&        path,
		const AssetTypeMask acceptedMask,
		const char*         helpText
	) {
		bool changed  = false;
		bool rejected = false;

		ImGui::PushID(label);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();

		constexpr float buttonWidth = 36.0f;
		const float     helpWidth   =
			helpText && helpText[0] != '\0' ? 24.0f : 0.0f;
		const float inputWidth = std::max(
			80.0f,
			ImGui::GetContentRegionAvail().x - buttonWidth - helpWidth -
			ImGui::GetStyle().ItemSpacing.x * 2.0f
		);

		std::array<char, 512> pathBuffer = {};
		memcpy(
			pathBuffer.data(),
			path.c_str(),
			std::min(path.size(), pathBuffer.size() - 1)
		);
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::InputText(
			"##AssetPath",
			pathBuffer.data(),
			pathBuffer.size()
		)) {
			if (TryCommitAssetPath(
				path,
				Path(pathBuffer.data()),
				acceptedMask
			)) {
				changed = true;
			} else {
				rejected = true;
			}
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
				kAssetDragDropPayloadId
			)) {
				if (payload->DataSize == sizeof(AssetDragDropPayload)) {
					const auto* assetPayload = static_cast<const
						AssetDragDropPayload*>(
						payload->Data
					);
					if (
						TryCommitAssetPath(
							path,
							Path(assetPayload->path),
							acceptedMask
						)
					) {
						changed = true;
					} else {
						rejected = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		const ImGuiID     widgetId    = ImGui::GetID("##AssetPath");
		BrowserViewState& pickerState = sPickerStates[widgetId];
		(void)EnsureActiveMount(pickerState);

		ImGui::SameLine();
		const std::string popupId = std::format(
			"AssetPickerPopup##{}",
			static_cast<uint32_t>(widgetId)
		);
		if (ImGui::Button("...")) {
			const Path rootPath    = pickerState.rootPath.LexicallyNormal();
			const auto virtualPath = VirtualPath::ParseContentReference(path);
			const auto resolved    = virtualPath && GetContentResolver() ?
				                         GetContentResolver()->ResolveFile(
					                         *virtualPath
				                         ) :
				                         std::nullopt;
			if (resolved && SetActiveMount(pickerState, resolved->mountId)) {
				pickerState.currentPath =
					resolved->resolvedPath.ParentPath().LexicallyNormal();
				pickerState.selectedPath = resolved->resolvedPath.
					LexicallyNormal();
			} else {
				pickerState.currentPath = rootPath.LexicallyNormal();
				pickerState.selectedPath.Clear();
			}
			ImGui::OpenPopup(popupId.c_str());
		}

		if (helpText && helpText[0] != '\0') {
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", helpText);
			}
		}

		if (rejected) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
				"Type mismatch: selected asset is not accepted."
			);
		}

		constexpr float windowWidth  = 1024.0f;
		constexpr float windowHeight = 768.0f;
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

		if (
			ImGui::BeginPopupModal(
				popupId.c_str(),
				nullptr,
				ImGuiWindowFlags_NoSavedSettings
			)
		) {
			(void)DrawTopBar(pickerState);

			std::string committedPath;
			const bool  committedByDoubleClick = DrawContentView(
				pickerState,
				acceptedMask,
				false,
				false,
				&committedPath,
				nullptr
			);
			if (committedByDoubleClick && TryCommitAssetPath(
				    path,
				    Path(committedPath),
				    acceptedMask
			    )) {
				changed = true;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Select")) {
				if (
					!pickerState.selectedPath.IsEmpty() &&
					TryCommitAssetPath(
						path,
						pickerState.selectedPath,
						acceptedMask
					)
				) {
					changed = true;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return changed;
	}

	void DrawWindow(BrowserViewState& state, const char* windowName) {
		DrawWindow(state, windowName, {});
	}

	void DrawWindow(
		BrowserViewState&        state,
		const char*              windowName,
		const AssetOpenCallback& onAssetOpen
	) {
		if (!ImGui::Begin(
			windowName,
			nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
		)) {
			ImGui::End();
			return;
		}

		if (!EnsureActiveMount(state)) {
			ImGui::TextUnformatted("No content mounts are available.");
			ImGui::End();
			return;
		}
		const fs::path  rootPath = state.rootPath.LexicallyNormal().Native();
		std::error_code ec;
		if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) {
			ImGui::TextUnformatted(
				"Selected content mount directory was not found."
			);
			ImGui::End();
			return;
		}

		(void)DrawTopBar(state);
		(void)DrawContentView(
			state,
			kAssetTypeMaskAny,
			true,
			true,
			nullptr,
			&onAssetOpen
		);
		ImGui::End();
	}
}

#endif
