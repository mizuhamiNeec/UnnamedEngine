#ifdef _DEBUG
#include "GuiEditorTool.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"

#include "engine/gui/UiCanvasRuntime.h"
#include "engine/render/Renderer.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	GuiEditorTool::GuiEditorTool(ImGuiLayer& imGuiLayer)
		: mImGuiLayer(imGuiLayer) {
	}

	void GuiEditorTool::Initialize(const EditorToolServices& services) {
		if (mInitialized) {
			return;
		}
		if (!services.assetManager) {
			Error(kChannelNone, "AssetManager is not available for GUI Editor.");
			return;
		}

		mAssetManager = services.assetManager;
		mGuiDocumentManager = std::make_unique<Gui::UiDocumentManager>(
			*mAssetManager
		);
		mGuiEditorRoot        = std::make_unique<Gui::UiRoot>();
		mGuiEditorScreenStack = std::make_unique<Gui::UiScreenStack>(
			mGuiEditorRoot.get()
		);
		mGuiEditorContext = std::make_unique<Gui::GuiEditorContext>();
		mInitialized      = true;
	}

	void GuiEditorTool::Shutdown() {
		mGuiPreviewSprites.clear();
		mGuiPreviewDrawCommands.clear();
		mGuiEditorContext.reset();
		mGuiEditorScreenStack.reset();
		mGuiEditorRoot.reset();
		mGuiActiveDocument.reset();
		mGuiDocumentManager.reset();
		mAssetManager = nullptr;
		mGuiEditorDockInitialized = false;
		mInitialized              = false;
	}

	void GuiEditorTool::Tick(const EditorToolFrameContext&) {
	}

	void GuiEditorTool::BuildUi(const EditorToolFrameContext& frameContext) {
		if (!mOpen) {
			mGuiEditorDockInitialized = false;
			return;
		}
		if (
			!mInitialized ||
			!mGuiDocumentManager ||
			!mGuiEditorRoot ||
			!mGuiEditorScreenStack ||
			!mGuiEditorContext
		) {
			return;
		}

		const float deltaTime = frameContext.unscaledDeltaTime;

		(void)mGuiDocumentManager->UpdateTrackedDocuments();
		if (!mGuiEditorContext->activeDocumentPath.IsEmpty()) {
			if (auto latest = mGuiDocumentManager->GetDocument(
					mGuiEditorContext->activeDocumentPath
				);
				latest && latest != mGuiActiveDocument) {
				mGuiActiveDocument                = latest;
				mGuiEditorContext->selectedWidget = nullptr;
				mGuiEditorScreenStack->Clear();
				mGuiEditorScreenStack->PushScreen(
					std::make_shared<Gui::UiScreen>(latest)
				);
			}
		}
		mGuiEditorContext->documentChanged = false;

		constexpr ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoCollapse;
		if (!ImGui::Begin("GUI Editor", &mOpen, hostFlags)) {
			ImGui::End();
			return;
		}
		const ImVec2  dockNodeSize = ImGui::GetContentRegionAvail();
		const ImGuiID dockSpaceId  = ImGui::GetID("GUIEditorDockSpace");
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f),
		                 ImGuiDockNodeFlags_None);
		if (!mGuiEditorDockInitialized) {
			if (dockNodeSize.x > 1.0f && dockNodeSize.y > 1.0f) {
				ImGui::DockBuilderRemoveNode(dockSpaceId);
				ImGui::DockBuilderAddNode(
					dockSpaceId,
					ImGuiDockNodeFlags_DockSpace
				);
				ImGui::DockBuilderSetNodeSize(
					dockSpaceId,
					ImVec2(std::max(1.0f, dockNodeSize.x),
					       std::max(1.0f, dockNodeSize.y))
				);
				ImGuiID dockMain = dockSpaceId;
				ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
					dockMain,
					ImGuiDir_Left,
					0.25f,
					nullptr,
					&dockMain
				);
				const ImGuiID dockRight = ImGui::DockBuilderSplitNode(
					dockMain,
					ImGuiDir_Right,
					0.30f,
					nullptr,
					&dockMain
				);
				const ImGuiID dockLeftBottom = ImGui::DockBuilderSplitNode(
					dockLeft,
					ImGuiDir_Down,
					0.45f,
					nullptr,
					&dockLeft
				);
				const ImGuiID dockOutliner = ImGui::DockBuilderSplitNode(
					dockLeft,
					ImGuiDir_Down,
					0.50f,
					nullptr,
					&dockLeft
				);
				ImGui::DockBuilderDockWindow("UI Document", dockLeft);
				ImGui::DockBuilderDockWindow("Ui Outliner", dockOutliner);
				ImGui::DockBuilderDockWindow("Ui Palette", dockLeftBottom);
				ImGui::DockBuilderDockWindow("Ui Inspector", dockRight);
				ImGui::DockBuilderDockWindow("Ui Preview", dockMain);
				ImGui::DockBuilderFinish(dockSpaceId);
				mGuiEditorDockInitialized = true;
			}
		}
		const ImGuiCond dockCond = mGuiEditorDockInitialized ?
			                           ImGuiCond_FirstUseEver :
			                           ImGuiCond_Always;
		ImGui::SetNextWindowDockID(dockSpaceId, dockCond);
		Gui::DrawUiEditorMenu(
			*mGuiDocumentManager,
			mGuiActiveDocument,
			mGuiEditorScreenStack.get(),
			*mGuiEditorContext
		);
		ImGui::SetNextWindowDockID(dockSpaceId, dockCond);
		Gui::DrawUiHierarchyWindow(*mGuiEditorRoot, *mGuiEditorContext);
		ImGui::SetNextWindowDockID(dockSpaceId, dockCond);
		Gui::DrawUiPaletteWindow(*mGuiEditorRoot, *mGuiEditorContext);
		ImGui::SetNextWindowDockID(dockSpaceId, dockCond);
		Gui::DrawUiInspectorWindow(*mGuiEditorContext, *mAssetManager);

		if (mGuiEditorContext->documentChanged) {
			const Path trackingPath =
				!mGuiEditorContext->activeDocumentPath.IsEmpty() ?
					mGuiEditorContext->activeDocumentPath :
					Path(mGuiEditorContext->pathBuffer.data()).
					LexicallyNormal();
			mGuiDocumentManager->MarkDirty(trackingPath, true);
		}

		mGuiEditorRoot->SetRootSize(
			std::max(1.0f, mGuiPreviewResolution.x),
			std::max(1.0f, mGuiPreviewResolution.y)
		);
		mGuiEditorScreenStack->Tick(deltaTime);
		mGuiEditorRoot->Tick(deltaTime);
		mGuiEditorRoot->UpdateLayout();

		Render::SceneOutputView previewOutput = {};
		Vec2                    previewSize   = Vec2::zero;
		if (const auto it = mViewOutputs.find(std::string(kViewGuiPreview));
			it != mViewOutputs.end()) {
			previewOutput.textureId   = it->second.textureId;
			previewOutput.srvCpu      = it->second.srvCpu;
			previewOutput.srvRevision = it->second.srvRevision;
			previewSize               = it->second.size;
		}

		ImGui::SetNextWindowDockID(dockSpaceId, dockCond);
		Gui::DrawUiPreviewWindow(
			mGuiEditorRoot.get(),
			previewOutput,
			previewSize,
			mImGuiLayer,
			*mGuiEditorContext,
			mGuiPreviewResolution
		);
		ImGui::End();

		mGuiPreviewDrawCommands.clear();
		mGuiEditorRoot->BuildDrawCommands(mGuiPreviewDrawCommands);

		mGuiPreviewSprites.clear();
		mGuiPreviewSprites.reserve(mGuiPreviewDrawCommands.size());
		for (size_t i = 0; i < mGuiPreviewDrawCommands.size(); ++i) {
			const auto& draw = mGuiPreviewDrawCommands[i];
			if (draw.type == Gui::UI_DRAW_COMMAND_TYPE::RECT) {
				mGuiPreviewSprites.emplace_back(
					UiCanvasRuntime::BuildScreenSprite(
						draw.rect, static_cast<int32_t>(i)
					)
				);
				continue;
			}

			if (draw.type == Gui::UI_DRAW_COMMAND_TYPE::IMAGE) {
				Render::ScreenSpriteInput sprite =
					UiCanvasRuntime::BuildScreenSprite(
						draw.image,
						static_cast<int32_t>(i)
					);
				mGuiPreviewSprites.emplace_back(std::move(sprite));
				continue;
			}

			if (!mGuiPreviewTextWarningLogged) {
				Warning(
					kChannelNone,
					"UiDrawCommand TEXT is not supported in GUI preview pass yet (Phase2)."
				);
				mGuiPreviewTextWarningLogged = true;
			}
		}
	}

	void GuiEditorTool::CollectRenderViews(Render::RenderFrameInputs& inputs) {
		if (!mOpen) {
			return;
		}

		Render::RenderViewInput previewView = {};
		previewView.viewKey = std::string(kViewGuiPreview);
		previewView.type = Render::RENDER_VIEW_TYPE::SPRITE_ONLY;
		previewView.output.sizeMode = Render::RENDER_VIEW_SIZE_MODE::FIXED;
		previewView.output.width = static_cast<uint32_t>(std::max(
			1.0f,
			mGuiPreviewResolution.x
		));
		previewView.output.height = static_cast<uint32_t>(std::max(
			1.0f,
			mGuiPreviewResolution.y
		));
		previewView.output.exposeToUi = true;
		previewView.screenSprites     = mGuiPreviewSprites;
		inputs.views.emplace_back(std::move(previewView));
	}

	void GuiEditorTool::EnumerateViewKeys(
		std::vector<std::string>& outViewKeys
	) const {
		if (!mOpen) {
			return;
		}
		outViewKeys.emplace_back(kViewGuiPreview);
	}

	void GuiEditorTool::SetViewOutput(
		const std::string_view         viewKey,
		const Render::SceneOutputView& output,
		const Vec2                     size
	) {
		if (viewKey != kViewGuiPreview) {
			return;
		}
		auto& cache       = mViewOutputs[std::string(viewKey)];
		cache.textureId   = output.textureId;
		cache.srvCpu      = output.srvCpu;
		cache.srvRevision = output.srvRevision;
		cache.size        = size;
	}

	bool GuiEditorTool::IsOpen() const {
		return mOpen;
	}

	void GuiEditorTool::SetOpen(const bool open) {
		if (mOpen != open) {
			mViewOutputs.clear();
		}
		mOpen = open;
		if (!mOpen) {
			mGuiEditorDockInitialized = false;
		}
	}

	std::string_view GuiEditorTool::GetToolId() const {
		return "tool.gui";
	}

	std::string_view GuiEditorTool::GetDisplayName() const {
		return "GUI Editor";
	}
}

#endif
