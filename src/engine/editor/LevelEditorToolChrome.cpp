#ifdef _DEBUG
#include "LevelEditorTool.h"
#include "core/filesystem/Path.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <utility>
#include <vector>

#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/EditorWorld.h"

namespace Unnamed {
	namespace {
		/// @brief 固定長バッファへ文字列を安全にコピーします。
		/// @param outBuffer コピー先バッファ
		/// @param value コピー元文字列
		void SetPathBuffer(
			std::array<char, 512>& outBuffer,
			const std::string_view value
		) {
			outBuffer.fill('\0');
			if (outBuffer.size() <= 1) {
				return;
			}

			const size_t copyLength = std::min(
				outBuffer.size() - 1,
				value.size()
			);
			std::copy_n(value.data(), copyLength, outBuffer.data());
			outBuffer[copyLength] = '\0';
		}

		[[nodiscard]] Path ResolveEditorSceneRootPath() {
			constexpr std::string_view kChannel           = "LevelEditorTool";
			constexpr std::string_view kFallbackSceneRoot =
				"./content/core/scenes";
			static bool sWarnedMissingModule        = false;
			static bool sWarnedUnavailableSceneRoot = false;

			const GameRuntimeContext* runtimeContext =
				ServiceLocator::Get<GameRuntimeContext>();
			if (!runtimeContext) {
				if (!sWarnedMissingModule) {
					Warning(
						kChannel,
						"GameRuntimeContext service was not available. Fallback scene root '{}'.",
						Path(kFallbackSceneRoot)
					);
					sWarnedMissingModule = true;
				}
				return Path(kFallbackSceneRoot);
			}

			const Path sceneRoot = ResolveGameContentPath(
				runtimeContext->modulePaths,
				Path("scenes")
			).LexicallyNormal();
			std::error_code ec;
			if (!sceneRoot.IsEmpty() &&
			    std::filesystem::exists(sceneRoot.Native(), ec) &&
			    !ec) {
				sWarnedUnavailableSceneRoot = false;
				return sceneRoot;
			}

			if (!sWarnedUnavailableSceneRoot) {
				Warning(
					kChannel,
					"Game scene root '{}' is unavailable. Fallback scene root '{}'.",
					sceneRoot,
					Path(kFallbackSceneRoot)
				);
				sWarnedUnavailableSceneRoot = true;
			}
			return Path(kFallbackSceneRoot);
		}

		/// @brief `<game content>/scenes/*.json` からシーン候補を収集します。
		/// @return ロード可能なシーンパス一覧
		[[nodiscard]] std::vector<Path> CollectSceneCandidates(
			const Path& scenesRoot
		) {
			namespace fs = std::filesystem;

			std::vector<Path> scenePaths = {};
			std::error_code   ec         = {};
			const fs::path    rootPath   = scenesRoot.Native();
			if (!fs::exists(rootPath, ec)) {
				return scenePaths;
			}

			fs::recursive_directory_iterator it(
				rootPath,
				fs::directory_options::skip_permission_denied,
				ec
			);
			const fs::recursive_directory_iterator endIt;
			for (; it != endIt; it.increment(ec)) {
				if (ec) {
					ec.clear();
					continue;
				}
				if (!it->is_regular_file(ec)) {
					continue;
				}

				const Path normalizedPath =
					Path::FromNative(it->path()).LexicallyNormal();
				if (normalizedPath.Extension() != Path(".json")) {
					continue;
				}

				scenePaths.emplace_back(std::move(normalizedPath));
			}

			std::ranges::sort(
				scenePaths,
				[](const Path& lhs, const Path& rhs) {
					return lhs.ToGenericUtf8() < rhs.ToGenericUtf8();
				}
			);
			scenePaths.erase(
				std::unique(scenePaths.begin(), scenePaths.end()),
				scenePaths.end()
			);
			return scenePaths;
		}
	}

	//-------------------------------------------------------------------------
	// Chromeは某ブラウザではなく、UIの外枠という意味なのじゃ。
	//-------------------------------------------------------------------------

	void LevelEditorTool::DrawMainMenu() {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				const Path currentPath      = mEditorWorld.GetLoadedScenePath();
				const Path sceneRoot        = ResolveEditorSceneRootPath();
				const Path defaultScenePath =
					(sceneRoot / Path("sandbox.json")).
					LexicallyNormal();

				if (ImGui::MenuItem("Save")) {
					const Path savePath = currentPath.IsEmpty() ?
						                      defaultScenePath :
						                      currentPath;
					if (SaveSceneAs(savePath)) {
						Msg(
							"LevelEditorTool",
							"Scene saved successfully: {}",
							savePath
						);
					} else {
						Warning(
							"LevelEditorTool",
							"Failed to save scene: {}",
							savePath
						);
					}
				}

				if (ImGui::MenuItem("Save As (sandbox.json)")) {
					if (SaveSceneAs(defaultScenePath)) {
						Msg(
							"LevelEditorTool",
							"Scene saved successfully: {}",
							defaultScenePath
						);
					} else {
						Warning(
							"LevelEditorTool",
							"Failed to save scene: {}",
							defaultScenePath
						);
					}
				}

				ImGui::Separator();

				if (ImGui::BeginMenu("Open Scene")) {
					static std::vector<Path> sSceneCandidates = {};
					static Path              sSceneRoot       = {};
					if (sSceneRoot != sceneRoot) {
						sSceneRoot       = sceneRoot;
						sSceneCandidates = CollectSceneCandidates(sceneRoot);
					}
					if (sSceneCandidates.empty()) {
						sSceneCandidates = CollectSceneCandidates(sceneRoot);
					}

					if (ImGui::MenuItem("Refresh Scene List")) {
						sSceneCandidates = CollectSceneCandidates(sceneRoot);
					}

					ImGui::Separator();

					constexpr size_t maxQuickOpenCount = 24;
					const size_t     quickOpenCount    = std::min(
						sSceneCandidates.size(),
						maxQuickOpenCount
					);
					if (quickOpenCount == 0) {
						ImGui::TextDisabled(
							"No scenes found in %s/**/*.json",
							sceneRoot.ToGenericUtf8().c_str()
						);
					} else {
						for (size_t i = 0; i < quickOpenCount; ++i) {
							const Path&       scenePath = sSceneCandidates[i];
							const std::string scenePathText =
								scenePath.ToGenericUtf8();
							if (!ImGui::MenuItem(scenePathText.c_str())) {
								continue;
							}
							SetPathBuffer(mOpenScenePathBuffer, scenePathText);
							(void)LoadSceneFromPath(scenePath);
						}
						if (sSceneCandidates.size() > quickOpenCount) {
							ImGui::TextDisabled(
								"... %zu more scenes (use path field below)",
								sSceneCandidates.size() - quickOpenCount
							);
						}
					}

					ImGui::Separator();

					const Path defaultPath = currentPath.IsEmpty() ?
						                         defaultScenePath :
						                         currentPath;
					if (mOpenScenePathBuffer[0] == '\0') {
						SetPathBuffer(
							mOpenScenePathBuffer,
							defaultPath.ToGenericUtf8()
						);
					}

					ImGui::SetNextItemWidth(360.0f);
					ImGui::InputText(
						"Path",
						mOpenScenePathBuffer.data(),
						mOpenScenePathBuffer.size()
					);

					if (ImGui::Button("Load")) {
						(void)LoadSceneFromPath(
							Path(mOpenScenePathBuffer.data())
						);
					}
					ImGui::SameLine();
					if (ImGui::Button("Use Current")) {
						SetPathBuffer(
							mOpenScenePathBuffer,
							defaultPath.ToGenericUtf8()
						);
					}

					ImGui::EndMenu();
				}

				ImGui::Separator();

				(void)ImGui::MenuItem("Import");
				(void)ImGui::MenuItem("Export");
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}

	void LevelEditorTool::DrawSideBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		constexpr float iconSize  = 40.0f;
		constexpr float iconScale = 0.8f;

		constexpr auto toolbarIconSize = ImVec2(iconSize, iconSize);

		const auto windowPadding = ImGui::GetStyle().WindowPadding;

		ImGui::SetNextWindowBgAlpha(1.0f);
		if (
			ImGui::BeginViewportSideBar(
				"##LeftToolbar", ImGui::GetMainViewport(), ImGuiDir_Left,
				toolbarIconSize.x + windowPadding.x * 2.0f, // アイコン幅 + 左右パディング
				flags
			)
		) {
			// ツールバー
			{
				ImGuiWidgets::IconButton(
					kIconSelect,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconDragPan,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconRotate,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconScale,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconPivot,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconObject,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconBox,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconTexture,
					"",
					toolbarIconSize,
					iconScale
				);
			}
			ImGui::End();
		}
	}

	void LevelEditorTool::DrawStatusBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		if (
			ImGui::BeginViewportSideBar(
				"##MainStatusBar",
				ImGui::GetMainViewport(),
				ImGuiDir_Down,
				32.0f,
				flags
			)
		) {
			const float statusBarWidth = ImGui::GetWindowWidth();

			// アングルスナップ
			{
				// 5.625° 360°の64分割
				// 11.25°は360°の32分割
				// 22.5°は360°の16分割
				constexpr std::array items = {
					"0.25°", "0.5°", "1°",
					"5°", "5.625°", "11.25°",
					"15°", "22.5°", "30°",
					"45°", "90°"
				};
				static uint32_t itemCurrentIndex = 6u; // デフォルトは15°
				const char*     comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("AngleSnapCombo");
				if (ImGui::BeginCombo("##angleSnap", comboLabel)) {
					for (int n = 0; n < items.size(); ++n) {
						const bool isSelected =
							std::cmp_equal(itemCurrentIndex, n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(items.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size
				);
				// 選択された文字列を浮動小数点数に変換してangleSnapに設定
				mAngleSnapDegree = std::stof(items[itemCurrentIndex]);
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::SameLine();

			// グリッドスナップ
			{
				constexpr std::array kGridSnapValues = {
					0.125f, 0.25f, 0.5f,
					1.0f, 2.0f, 4.0f, 8.0f,
					16.0f, 32.0f, 64.0f, 128.0f,
					256.0f, 512.0f, 1024.0f
				};
				constexpr std::array kGridSnapLabels = {
					"0.125", "0.25", "0.5",
					"1", "2", "4", "8",
					"16", "32", "64", "128",
					"256", "512", "1024"
				};

				auto FindNearestGridSnapIndex = [](
					const float valueInCurrentUnit
				) {
					uint32_t nearestIndex = 0u;
					float    nearestDiff  =
						std::abs(valueInCurrentUnit - kGridSnapValues[0]);
					for (uint32_t i = 1u; i < kGridSnapValues.size(); ++i) {
						const float diff =
							std::abs(valueInCurrentUnit - kGridSnapValues[i]);
						if (diff < nearestDiff) {
							nearestDiff  = diff;
							nearestIndex = i;
						}
					}
					return nearestIndex;
				};

				const bool  showHu = mGridSnapUnit == EDITOR_GRID_SNAP_UNIT::HU;
				const float gridSnapInCurrentUnit =
					showHu ? Math::MtoH(mGridSnap) : mGridSnap;
				uint32_t itemCurrentIndex = FindNearestGridSnapIndex(
					gridSnapInCurrentUnit
				);
				const auto& labels     = kGridSnapLabels;
				const char* comboLabel = labels[itemCurrentIndex];

				ImGui::Text("Grid: ");

				ImGui::SameLine();
				ImGui::PushItemWidth(statusBarWidth * 0.1f);
				if (ImGui::BeginCombo("##gridSnapUnit", showHu ? "Hu" : "m")) {
					const bool meterSelected =
						mGridSnapUnit == EDITOR_GRID_SNAP_UNIT::METER;
					if (ImGui::Selectable("m", meterSelected)) {
						mGridSnapUnit = EDITOR_GRID_SNAP_UNIT::METER;
					}
					if (meterSelected) {
						ImGui::SetItemDefaultFocus();
					}

					const bool huSelected =
						mGridSnapUnit == EDITOR_GRID_SNAP_UNIT::HU;
					if (ImGui::Selectable("Hu", huSelected)) {
						mGridSnapUnit = EDITOR_GRID_SNAP_UNIT::HU;
					}
					if (huSelected) {
						ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.16f);
				ImGui::PushID("GridSnapCombo");
				if (ImGui::BeginCombo("##gridSnap", comboLabel)) {
					for (int n = 0; n < labels.size(); ++n) {
						const bool isSelected =
							std::cmp_equal(itemCurrentIndex, n);
						if (ImGui::Selectable(labels[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(labels.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size

				);
				const float selectedGridSnap = kGridSnapValues[
					itemCurrentIndex];
				mGridSnap = showHu ?
					            Math::HtoM(selectedGridSnap) :
					            selectedGridSnap;
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::End();
		}
	}
}

#endif
