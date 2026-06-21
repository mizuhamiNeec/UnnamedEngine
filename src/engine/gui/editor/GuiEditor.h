#pragma once
#include <array>
#include <string>

#include "engine/gui/UiDocumentManager.h"
#include "engine/gui/UiScreenStack.h"

#include "core/math/Vec2.h"

#ifdef _DEBUG
namespace Unnamed {
	class ImGuiLayer;

	namespace Render {
		struct SceneOutputView;
	}
}

namespace Unnamed::Gui {
	class UiWidget;
	class UiRoot;

	struct GuiEditorContext {
		UiWidget*             selectedWidget       = nullptr;
		UiWidget*             renameTargetWidget   = nullptr;
		UiWidget*             pendingDeleteWidget  = nullptr;
		std::array<char, 256> pathBuffer           = {};
		std::array<char, 128> outlinerFilterBuffer = {};
		std::array<char, 128> renameBuffer         = {};
		Path                  activeDocumentPath;
		bool                  textWarningLogged     = false;
		bool                  documentChanged       = false;
		int                   addComponentTypeIndex = 0;
		float                 previewZoom           = 1.0f;
		bool                  previewOverlayEnabled = true;
	};

	void DrawUiHierarchyWindow(UiRoot& uiRoot, GuiEditorContext& context);
	void DrawUiPaletteWindow(UiRoot& uiRoot, GuiEditorContext& context);
	void DrawUiInspectorWindow(GuiEditorContext& context);
	void DrawUiEditorMenu(
		UiDocumentManager&           manager,
		std::shared_ptr<UiDocument>& activeDocument,
		UiScreenStack*               screenStack,
		GuiEditorContext&            context
	);
	void DrawUiPreviewWindow(
		UiRoot*                        uiRoot,
		const Render::SceneOutputView& previewOutput,
		const Vec2&                    previewSize,
		ImGuiLayer&                    imGuiLayer,
		GuiEditorContext&              context,
		const Vec2&                    canvasSize
	);
}
#endif
