#ifdef _DEBUG
#include "ContentBrowser.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "core/assets/AssetManager.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::EditorContentBrowser {
	namespace {
		namespace fs = std::filesystem;

		struct BrowserEntry {
			fs::path    path;
			std::string name;
			bool        isDirectory = false;
			ASSET_TYPE  type        = ASSET_TYPE::UNKNOWN;
		};

		std::unordered_map<ImGuiID, BrowserViewState> sPickerStates;

		std::string NormalizePath(const fs::path& path) {
			return StrUtil::NormalizePath(
				Path::ToGenericUtf8(path.lexically_normal())
			);
		}

		bool IsPathInsideRoot(const fs::path& path, const fs::path& root) {
			const std::string normalizedPath = NormalizePath(path);
			const std::string normalizedRoot = NormalizePath(root);
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
			const std::string&  candidate,
			const AssetTypeMask acceptedMask
		) {
			const std::string normalized = NormalizePath(
				Path::FromUtf8(candidate));
			if (normalized.empty()) {
				ioPath = normalized;
				return true;
			}

			const ASSET_TYPE guessedType = GuessAssetTypeFromPath(normalized);
			if (!IsAssetTypeAccepted(guessedType, acceptedMask)) {
				return false;
			}

			if (
				auto* assetManager = ServiceLocator::Get<AssetManager>();
				assetManager && guessedType != ASSET_TYPE::UNKNOWN
			) {
				(void)assetManager->LoadFromFile(normalized, guessedType);
			}
			ioPath = normalized;
			return true;
		}

		void EnumerateDirectories(
			const fs::path& currentDir, std::vector<fs::path>& outDirs
		) {
			outDirs.clear();
			std::error_code ec;
			for (fs::directory_iterator it(currentDir, ec), end;
			     !ec && it != end;
			     it.increment(ec)) {
				if (ec) {
					break;
				}
				if (!it->is_directory(ec) || ec) {
					continue;
				}
				outDirs.emplace_back(it->path());
			}
			std::ranges::sort(
				outDirs,
				[](const fs::path& lhs, const fs::path& rhs) {
					return Path::ToUtf8String(lhs.filename()) <
					       Path::ToUtf8String(rhs.filename());
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
				entry.path = de.path();
				entry.name = Path::ToUtf8String(entry.path.filename());
				entry.isDirectory = de.is_directory(ec);
				if (ec) {
					continue;
				}

				if (!entry.isDirectory) {
					entry.type = GuessAssetTypeFromPath(
						NormalizePath(entry.path)
					);
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
			const std::string normalizedPath = NormalizePath(entry.path);
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

		void DrawTreeRecursive(
			const fs::path& rootPath, const fs::path& nodePath,
			std::string&    currentPath
		) {
			const std::string node    = NormalizePath(nodePath);
			const std::string current = NormalizePath(
				Path::FromUtf8(currentPath));
			const bool isCurrent  = node == current;
			const bool isAncestor = current.starts_with(node) &&
			                        (current.size() == node.size() ||
			                         current[node.size()] == '/');

			std::vector<fs::path> children;
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
				                          Path::ToUtf8String(
					                          nodePath.filename());
			const bool opened = ImGui::TreeNodeEx(
				node.c_str(),
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
			for (const fs::path& child : children) {
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

			const std::string cellId = NormalizePath(entry.path);
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
				iconAreaSize * 0.42f, 18.0f, 34.0f
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
				entry.name, textMaxWidth
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
			std::string*             outCommittedPath,
			const AssetOpenCallback* onAssetOpen
		) {
			const fs::path rootPath = Path::FromUtf8(state.rootPath).
				lexically_normal();
			fs::path currentPath = Path::FromUtf8(state.currentPath).
				lexically_normal();
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
					"Tree", ImGuiTableColumnFlags_WidthFixed, 260.0f
				);
				ImGui::TableSetupColumn(
					"Main", ImGuiTableColumnFlags_WidthStretch, 0.0f
				);

				ImGui::TableNextColumn();
				ImGui::BeginChild(
					"##ContentBrowserTree", ImVec2(0.0f, 0.0f), false
				);
				std::string currentPathString = NormalizePath(currentPath);
				DrawTreeRecursive(rootPath, rootPath, currentPathString);
				currentPath = Path::FromUtf8(currentPathString).
					lexically_normal();
				ImGui::EndChild();

				ImGui::TableNextColumn();
				ImGui::BeginChild(
					"##ContentBrowserMain", ImVec2(0.0f, 0.0f), false
				);

				std::vector<BrowserEntry> entries;
				EnumerateEntries(currentPath, acceptedMask, entries);
				if (!state.iconView) {
					for (const BrowserEntry& entry : entries) {
						const std::string normalizedPath = NormalizePath(
							entry.path
						);
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
								currentPath = entry.path.lexically_normal();
							} else if (
								!entry.isDirectory &&
								ImGui::IsMouseDoubleClicked(
									ImGuiMouseButton_Left
								)
							) {
								if (outCommittedPath) {
									*outCommittedPath = normalizedPath;
									committed         = true;
								} else if (onAssetOpen && *onAssetOpen) {
									(*onAssetOpen)(normalizedPath, entry.type);
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
						const std::string normalizedPath = NormalizePath(
							entry.path
						);
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
								currentPath = entry.path.lexically_normal();
							} else if (
								!entry.isDirectory && doubleClicked
							) {
								if (outCommittedPath) {
									*outCommittedPath = normalizedPath;
									committed         = true;
								} else if (onAssetOpen && *onAssetOpen) {
									(*onAssetOpen)(normalizedPath, entry.type);
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

				ImGui::EndChild();
				ImGui::EndTable();
			}

			state.currentPath = NormalizePath(currentPath);
			return committed;
		}

		bool DrawTopBar(BrowserViewState& state) {
			const fs::path rootPath = Path::FromUtf8(state.rootPath).
				lexically_normal();
			fs::path currentPath = Path::FromUtf8(state.currentPath).
				lexically_normal();
			if (!IsPathInsideRoot(currentPath, rootPath)) {
				currentPath = rootPath;
			}

			if (
				ImGuiWidgets::IconButton(
					kIconArrowUpward, nullptr, ImVec2(0, 0), 1.0f
				)
			) {
				const fs::path parent = currentPath.parent_path();
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
			ImGui::TextDisabled("%s", NormalizePath(currentPath).c_str());

			state.currentPath = NormalizePath(currentPath);
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
			(helpText && helpText[0] != '\0') ? 24.0f : 0.0f;
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
			"##AssetPath", pathBuffer.data(), pathBuffer.size()
		)) {
			if (TryCommitAssetPath(path, pathBuffer.data(), acceptedMask)) {
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
					if (TryCommitAssetPath(
						path, assetPayload->path, acceptedMask
					)) {
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
		if (pickerState.rootPath.empty()) {
			pickerState.rootPath = "./content";
		}
		if (pickerState.currentPath.empty()) {
			pickerState.currentPath = pickerState.rootPath;
		}

		ImGui::SameLine();
		const std::string popupId = std::format(
			"AssetPickerPopup##{}",
			static_cast<uint32_t>(widgetId)
		);
		if (ImGui::Button("...")) {
			const fs::path rootPath = Path::FromUtf8(pickerState.rootPath).
				lexically_normal();
			const fs::path targetPath = Path::FromUtf8(path).lexically_normal();
			if (!path.empty() && IsPathInsideRoot(targetPath, rootPath)) {
				pickerState.currentPath = NormalizePath(
					targetPath.parent_path()
				);
				pickerState.selectedPath = NormalizePath(targetPath);
			} else {
				pickerState.currentPath = NormalizePath(rootPath);
				pickerState.selectedPath.clear();
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
				&committedPath,
				nullptr
			);
			if (committedByDoubleClick && TryCommitAssetPath(
				    path, committedPath, acceptedMask
			    )) {
				changed = true;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Select")) {
				if (
					!pickerState.selectedPath.empty() &&
					TryCommitAssetPath(
						path, pickerState.selectedPath, acceptedMask
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
			windowName, nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
		)) {
			ImGui::End();
			return;
		}

		const fs::path rootPath = Path::FromUtf8(state.rootPath).
			lexically_normal();
		std::error_code ec;
		if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) {
			ImGui::TextUnformatted("`./content` directory not found.");
			ImGui::End();
			return;
		}

		(void)DrawTopBar(state);
		(void)DrawContentView(
			state,
			kAssetTypeMaskAny,
			true,
			nullptr,
			&onAssetOpen
		);
		ImGui::End();
	}
}

#endif
