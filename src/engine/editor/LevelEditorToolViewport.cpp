#ifdef _DEBUG
#include <algorithm>
#include <cmath>
#include <format>
#include <imgui.h>

#include "LevelEditorTool.h"

#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/world/EditorWorld.h"

#include "sequence/SequenceEditorController.h"

#include "thirdparty/ImGuizmo/ImGuizmo.h"

namespace Unnamed {
	namespace {
		/// @brief ViewportFitResultは、panel内へaspect比を保って収めた描画位置、寸法、aspect比を返します
		struct ViewportFitResult {
			Vec2  drawPos     = Vec2::zero;
			Vec2  drawSize    = Vec2::one;
			float aspectRatio = 1.0f;
		};

		[[nodiscard]] ViewportFitResult ComputeViewportFitResult(
			const Vec2& panelScreenPos,
			const Vec2& panelSize,
			const Vec2& contentLogicalSize
		) {
			const float safePanelWidth = std::max(1.0f, panelSize.x);
			const float safePanelHeight = std::max(1.0f, panelSize.y);
			const float safeContentWidth = std::max(1.0f, contentLogicalSize.x);
			const float safeContentHeight = std::max(
				1.0f, contentLogicalSize.y
			);

			ViewportFitResult result;
			result.aspectRatio = safeContentWidth / safeContentHeight;

			float fitWidth  = safePanelWidth;
			float fitHeight = fitWidth / result.aspectRatio;
			if (fitHeight > safePanelHeight) {
				fitHeight = safePanelHeight;
				fitWidth  = fitHeight * result.aspectRatio;
			}

			fitWidth               = std::max(1.0f, fitWidth);
			fitHeight              = std::max(1.0f, fitHeight);
			const float fitOffsetX = (safePanelWidth - fitWidth) * 0.5f;
			const float fitOffsetY = (safePanelHeight - fitHeight) * 0.5f;
			result.drawPos         = Vec2(
				panelScreenPos.x + fitOffsetX, panelScreenPos.y + fitOffsetY
			);
			result.drawSize = Vec2(fitWidth, fitHeight);
			return result;
		}
	}

	void LevelEditorTool::DrawViewport(const float deltaTime) {
		if (!ImGui::Begin("Viewport")) {
			ImGui::End();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		DrawViewportTopBar();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		const ImVec2 avail            = ImGui::GetContentRegionAvail();
		const float  width            = std::max(1.0f, avail.x);
		const float  height           = std::max(1.0f, avail.y);
		mViewportSizeChangedThisFrame =
			std::abs(width - mLastViewportSize.x) > 0.5f ||
			std::abs(height - mLastViewportSize.y) > 0.5f;
		mViewportPanelWidth  = width;
		mViewportPanelHeight = height;
		mLastViewportSize    = Vec2(width, height);

		auto DrawPane = [this](
			const std::string_view viewKey,
			const char*            label,
			const float            drawWidth,
			const float            drawHeight
		) {
			const ImVec2 panePos    = ImGui::GetCursorScreenPos();
			const auto   outputIt   = mViewOutputs.find(std::string(viewKey));
			const bool   hasOutput  = outputIt != mViewOutputs.end();
			const Vec2   outputSize = hasOutput ?
				                        outputIt->second.size :
				                        Vec2(drawWidth, drawHeight);
			const ViewportFitResult fit = ComputeViewportFitResult(
				Vec2(panePos.x, panePos.y),
				Vec2(drawWidth, drawHeight),
				outputSize
			);
			const ImVec2 fitPos(fit.drawPos.x, fit.drawPos.y);
			const ImVec2 fitMax(
				fit.drawPos.x + fit.drawSize.x, fit.drawPos.y + fit.drawSize.y
			);

			// ImGuizmo のドラッグ入力を奪わないよう、ここはレイアウト確保のみ行う
			ImGui::Dummy(ImVec2(drawWidth, drawHeight));
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(
				panePos,
				ImVec2(panePos.x + drawWidth, panePos.y + drawHeight),
				IM_COL32(28, 30, 36, 255),
				8.0f
			);
			drawList->AddRect(
				panePos,
				ImVec2(panePos.x + drawWidth, panePos.y + drawHeight),
				IM_COL32(70, 74, 86, 255),
				8.0f
			);

			if (
				hasOutput &&
				outputIt->second.srvCpu.ptr != 0 &&
				outputIt->second.textureId != 0
			) {
				const ImTextureID tex = mImGuiLayer.GetOrCreateTextureId(
					outputIt->second.textureId,
					outputIt->second.srvRevision,
					outputIt->second.srvCpu
				);
				ImGui::SetCursorScreenPos(fitPos);
				ImGuiWidgets::ImageWithRounding(
					tex,
					ImVec2(fit.drawSize.x, fit.drawSize.y),
					4.0f,
					ImDrawFlags_RoundCornersAll,
					ImVec2(outputIt->second.uvMin.x, outputIt->second.uvMin.y),
					ImVec2(outputIt->second.uvMax.x, outputIt->second.uvMax.y)
				);
			} else {
				drawList->AddRect(
					fitPos,
					fitMax,
					IM_COL32(130, 130, 130, 255),
					0.0f
				);
			}

			const ImVec2 mousePos   = ImGui::GetMousePos();
			const bool   hoveredFit =
				mousePos.x >= fitPos.x && mousePos.x <= fitMax.x &&
				mousePos.y >= fitPos.y && mousePos.y <= fitMax.y &&
				ImGui::IsWindowHovered(
					ImGuiHoveredFlags_AllowWhenBlockedByPopup
				);
			if (hoveredFit && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				mActiveViewportViewKey = std::string(viewKey);
				mViewportLookActive    = true;
			}
			if (
				hoveredFit ||
				(!mActiveViewportViewKey.empty() && mActiveViewportViewKey ==
				 viewKey)
			) {
				mViewportPosition = Vec2(fitPos.x, fitPos.y);
				mViewportSize     = fit.drawSize;
			}

			(void)hoveredFit;

			drawList->AddText(
				ImVec2(panePos.x + 8.0f, panePos.y + 8.0f),
				IM_COL32(240, 240, 240, 255),
				label
			);

			return std::tuple<bool, Vec2, Vec2>(
				hoveredFit,
				Vec2(fitPos.x, fitPos.y),
				fit.drawSize
			);
		};

		const auto [hovered, pos, size] = DrawPane(
			kViewScenePerspective,
			"Perspective",
			width,
			height
		);
		(void)hovered;
		const Render::SceneViewRenderMode mode = BuildSceneViewModeForSize(
			size.x,
			size.y,
			false
		);
		DrawViewportGizmo(
			kViewScenePerspective, mode, pos, size.x, size.y
		);

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			mViewportLookActive = false;
		}

		DrawViewportOverlay(deltaTime);

		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	void LevelEditorTool::DrawViewportTopBar() {
		const float toolbarHeight            = ImGui::GetFontSize() * 2.0f;
		const float toolbarHeightWithPadding =
			toolbarHeight +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		auto currentCursorPos = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRectFilled(
			currentCursorPos,
			ImVec2(
				currentCursorPos.x + ImGui::GetContentRegionAvail().x,
				currentCursorPos.y + ImGui::GetContentRegionAvail().y
			),
			ImGui::GetColorU32(ImGuiCol_ChildBg),
			ImGui::GetStyle().ChildRounding,
			ImDrawFlags_RoundCornersAll
		);

		ImGui::SetCursorScreenPos(
			ImVec2(
				currentCursorPos.x + ImGui::GetStyle().WindowPadding.x,
				currentCursorPos.y + ImGui::GetStyle().WindowPadding.y
			)
		);

		auto SubmitWarningMessage = [this](const std::string& message) {
			mConsoleSystem->ExecuteCommand(
				"notify warn 10 未実装 | " + message
			);
		};

		{
			const auto toolbarIconSize = ImVec2(
				toolbarHeight * 2.75f,
				toolbarHeight
			);

			constexpr float iconScale = 1.0f;

			if (
				ImGuiWidgets::IconButton(
					kIconVertex,
					"Vertices",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("頂点選択は未実装です。");
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconEdge,
					"Edges",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("辺選択は未実装です。");
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconFace,
					"Faces",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("面選択は未実装です。");
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconMesh,
					"Meshes",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("メッシュ選択は未実装です。");
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconObject,
					"Object",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("オブジェクト選択は未実装です。");
			}
			ImGui::SameLine();
			if (
				ImGuiWidgets::IconButton(
					kIconGroup,
					"Group",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				SubmitWarningMessage("グループ選択は未実装です。");
			}
		}

		ImGui::SameLine();
		{
			auto viewportModeLabel = "Fit";
			switch (mViewportRenderMode) {
				case EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
				: viewportModeLabel = "Fit";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
				: viewportModeLabel = "16:9";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3
				: viewportModeLabel = "4:3";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::HD720
				: viewportModeLabel = "HD";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::FHD1080
				: viewportModeLabel = "FHD";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::UHD4K
				: viewportModeLabel = "UHD4K";
				default: break;
			}
			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::BeginCombo("RenderMode", viewportModeLabel)) {
				if (ImGui::Selectable(
					"Fit",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
				}
				if (ImGui::Selectable(
					"Fixed 16:9",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9;
				}
				if (ImGui::Selectable(
					"Fixed 4:3",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3;
				}
				if (ImGui::Selectable(
					"HD (1280x720)",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::HD720
				)) {
					mViewportRenderMode = EDITOR_VIEWPORT_RENDER_MODE::HD720;
				}
				if (ImGui::Selectable(
					"FHD (1920x1080)",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FHD1080
				)) {
					mViewportRenderMode = EDITOR_VIEWPORT_RENDER_MODE::FHD1080;
				}
				if (ImGui::Selectable(
					"UHD4K",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::UHD4K
				)) {
					mViewportRenderMode = EDITOR_VIEWPORT_RENDER_MODE::UHD4K;
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			auto DrawViewBindingSelector = [this] {
				const Scene*          scene   = GetOutlinerScene();
				ViewportCameraBinding binding = ResolveViewportBinding(
					kViewScenePerspective
				);
				auto currentKindLabel = "EditorPerspective";
				switch (binding.kind) {
					case ViewportCameraBindingKind::EditorPerspective
					: currentKindLabel = "EditorPerspective";
						break;
					case ViewportCameraBindingKind::ActiveGameCamera
					: currentKindLabel = "ActiveGameCamera";
						break;
					case ViewportCameraBindingKind::CameraEntity
					: currentKindLabel = "CameraEntity";
						break;
					default: break;
				}

				ImGui::SetNextItemWidth(180.0f);
				if (ImGui::BeginCombo("Binding", currentKindLabel)) {
					const auto SelectableKind = [&](
						const char*                     label,
						const ViewportCameraBindingKind kind
					) {
						if (ImGui::Selectable(label, binding.kind == kind)) {
							ViewportCameraBinding next = binding;
							next.kind                  = kind;
							if (kind !=
							    ViewportCameraBindingKind::CameraEntity) {
								next.cameraEntityGuid = 0;
							}
							mCameraManager.SetPaneBinding(
								kViewScenePerspective, next
							);
							binding = next;
						}
					};

					SelectableKind(
						"EditorPerspective",
						ViewportCameraBindingKind::EditorPerspective
					);
					SelectableKind(
						"ActiveGameCamera",
						ViewportCameraBindingKind::ActiveGameCamera
					);
					SelectableKind(
						"CameraEntity",
						ViewportCameraBindingKind::CameraEntity
					);
					ImGui::EndCombo();
				}

				if (binding.kind == ViewportCameraBindingKind::CameraEntity) {
					std::string cameraLabel = "<none>";
					if (scene && binding.cameraEntityGuid != 0) {
						for (const auto& entity : scene->GetEntities()) {
							if (!entity || entity->GetGuid() != binding.
							    cameraEntityGuid) {
								continue;
							}
							cameraLabel = std::string(entity->GetName());
							break;
						}
					}

					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::BeginCombo(
						"CameraEntity", cameraLabel.c_str()
					)) {
						if (ImGui::Selectable(
							"<none>", binding.cameraEntityGuid == 0
						)) {
							ViewportCameraBinding next = binding;
							next.cameraEntityGuid      = 0;
							mCameraManager.SetPaneBinding(
								kViewScenePerspective, next
							);
							binding = next;
						}
						if (scene) {
							for (const auto& entity : scene->GetEntities()) {
								if (!entity || !entity->IsActive()) {
									continue;
								}
								const auto* camera = entity->GetComponent<
									CameraComponent>();
								if (
									!camera ||
									!camera->IsActive() ||
									!camera->IsCameraActive()
								) {
									continue;
								}
								const bool selected =
									entity->GetGuid() == binding.
									cameraEntityGuid;
								const std::string label = std::format(
									"{}##{}",
									entity->GetName(),
									entity->GetGuid()
								);
								if (ImGui::Selectable(
									label.c_str(), selected
								)) {
									ViewportCameraBinding next = binding;
									next.cameraEntityGuid = entity->GetGuid();
									mCameraManager.SetPaneBinding(
										kViewScenePerspective, next
									);
									binding = next;
								}
							}
						}
						ImGui::EndCombo();
					}
				}
			};
			DrawViewBindingSelector();
		}

		ImGui::SetCursorScreenPos(
			{
				currentCursorPos.x,
				currentCursorPos.y + toolbarHeightWithPadding
			}
		);
	}

	void LevelEditorTool::DrawViewportGizmo(
		const std::string_view             viewKey,
		const Render::SceneViewRenderMode& sceneViewMode,
		const Vec2&                        imagePos,
		const float                        drawWidth,
		const float                        drawHeight
	) {
		Entity* entity = GetSelectedEntity();
		if (!entity) {
			return;
		}

		auto* transform = entity->GetComponent<TransformComponent>();
		if (!transform) {
			return;
		}
		Mat4 world = transform->GetWorldMat();

		const EditorViewportCameraManager::ResolvedCamera camera =
			mCameraManager.
			ResolveViewCamera(
				mEditorWorld,
				viewKey,
				sceneViewMode,
				ResolveViewportBinding(viewKey),
				nullptr
			);
		if (!camera.input.valid) {
			return;
		}
		const Mat4 view = camera.input.view;
		const Mat4 proj = camera.input.proj;

		static ImGuizmo::OPERATION sOperation = ImGuizmo::TRANSLATE;
		static ImGuizmo::MODE      sMode      = ImGuizmo::WORLD;

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_W)) {
				sOperation = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E)) {
				sOperation = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R)) {
				sOperation = ImGuizmo::SCALE;
			}
		}

		ImGuizmo::SetOrthographic(camera.isOrthographic);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(imagePos.x, imagePos.y, drawWidth, drawHeight);

		const bool useSnap = sOperation != ImGuizmo::SCALE &&
		                     (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
		                      ImGui::IsKeyDown(ImGuiKey_RightCtrl));
		const float snap[3] = {
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap
		};

		if (
			ImGuizmo::Manipulate(
				*view.m,
				*proj.m,
				sOperation,
				sMode,
				*world.m,
				nullptr,
				useSnap ? snap : nullptr
			)
		) {
			Mat4 localMat = world;
			if (const TransformComponent* parent = transform->GetParent()) {
				// ImGuizmo が編集したワールド行列を、親基準のローカル行列へ戻す
				localMat = world * parent->GetWorldMat().Inverse();
			}

			float translation[3] = {};
			float rotation[3]    = {};
			float scale[3]       = {};
			ImGuizmo::DecomposeMatrixToComponents(
				*localMat.m,
				translation,
				rotation,
				scale
			);

			transform->SetPosition(
				Vec3(translation[0], translation[1], translation[2])
			);
			transform->SetRotation(
				Quaternion::EulerDegrees(
					Vec3(rotation[0], rotation[1], rotation[2])
				)
			);
			transform->SetScale(Vec3(scale[0], scale[1], scale[2]));
			transform->RequestInterpolationResync();
			if (mSequenceEditorController) {
				mSequenceEditorController->OnGizmoTransformChanged(
					entity->GetGuid(),
					transform->GetPosition(),
					transform->GetRotation(),
					transform->GetScale()
				);
			}
		}
	}

	void LevelEditorTool::DrawViewportOverlay(const float deltaTime) const {
		const EditorCameraComponent* camera = mEditorWorld.GetEditorCamera();
		if (!camera || !camera->IsMoveSpeedPopupVisible()) {
			return;
		}

		constexpr float windowMinWidth = 256.0f;

		const std::string text =
			StrUtil::ConvertToUtf8(kIconSpeed) +
			std::format(" {:.2f}", camera->GetMoveSpeed());

		const auto textSize      = ImGui::CalcTextSize(text.c_str());
		const auto windowPadding = ImGui::GetStyle().WindowPadding;

		const ImVec2 windowSize(
			std::max(textSize.x, windowMinWidth) + windowPadding.x,
			textSize.y + windowPadding.y
		);

		ImVec2 windowPos(
			mViewportPosition.x + mViewportSize.x * 0.5f,
			mViewportPosition.y + mViewportSize.y * 0.9f
		);
		windowPos.x -= windowSize.x * 0.5f;
		windowPos.y -= windowSize.y - 8.0f;

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		constexpr float fadeOutStart       = 0.5f;
		constexpr float invFadeOutDuration = 1.0f / (1.0f - fadeOutStart);

		const float alphaMultiplier =
			camera->GetMoveSpeedPopupTimer() < fadeOutStart ?
				1.0f :
				1.0f - (camera->GetMoveSpeedPopupTimer() - fadeOutStart) *
				invFadeOutDuration;

		const float alpha = Math::CubicBezier(
			alphaMultiplier,
			0.2f, 0.0f, 0.0f, 1.0f
		);

		ImGui::SetNextWindowBgAlpha(alpha * 0.9f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)
		);

		ImGui::Begin(
			"##editorCameraMoveSpeedPopup", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs
		);

		ImGui::SetCursorPos(
			ImVec2(
				(windowSize.x - textSize.x) * 0.5f,
				(windowSize.y - textSize.y) * 0.25f
			)
		);
		ImGui::TextUnformatted(text.c_str());

		static float currentProgress = 0.0f;
		float        progress;
		if (camera->GetMoveSpeed() <= 0.125f) {
			progress = 0.0f;
		} else if (camera->GetMoveSpeed() >= 65536.0f) {
			progress = 1.0f;
		} else {
			const float logMin   = std::log2(0.125f);
			const float logMax   = std::log2(65536.0f);
			const float logValue = std::log2(camera->GetMoveSpeed());
			progress             = (logValue - logMin) / (logMax - logMin);
		}

		const float t = Math::CubicBezier(
			20.0f * deltaTime, 0.2f, 0.0f, 0.0f, 1.0f
		);

		currentProgress = std::lerp(currentProgress, progress, t);

		ImGui::SetCursorPosY(
			ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - 4.0f
		);

		ImGui::ProgressBar(
			currentProgress, ImVec2(
				ImGui::GetContentRegionAvail().x,
				4.0f
			),
			""
		);

		ImGui::End();
		ImGui::PopStyleVar(2);
	}
}

#endif
