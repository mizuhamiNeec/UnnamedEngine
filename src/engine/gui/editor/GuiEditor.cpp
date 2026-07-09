#include "GuiEditor.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <imgui.h>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/content/ContentPathResolver.h"
#include "core/string/StrUtil.h"

#include "engine/gui/UiRoot.h"
#include "engine/gui/UiScreenStack.h"
#include "engine/gui/UiWidget.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/gui/components/UiButtonBehaviorComponent.h"
#include "engine/gui/components/UiDigitStripComponent.h"
#include "engine/gui/components/UiLayoutComponents.h"
#include "engine/gui/components/UiPanelStyleComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/gui/components/UiTransformComponent.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/render/Renderer.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

#ifdef _DEBUG
namespace Unnamed::Gui {
	namespace {
		constexpr auto kRenamePopupId = "Rename Widget";

		struct PaletteTemplate {
			const char*                 category;
			const char*                 label;
			const char*                 tooltip;
			std::unique_ptr<UiWidget> (*factory)();
		};

		void CopyStringToBuffer(
			const std::string_view value,
			char*                  buffer,
			const size_t           bufferSize
		) {
			if (!buffer || bufferSize == 0) {
				return;
			}
			std::snprintf(buffer, bufferSize, "%s", std::string(value).c_str());
		}

		void MarkDocumentDirty(GuiEditorContext& context) {
			context.documentChanged = true;
		}

		void EnsureContextDefaults(GuiEditorContext& context) {
			if (context.pathBuffer[0] == '\0') {
				if (const auto* runtimeContext = ServiceLocator::Get<
					GameRuntimeContext>()) {
					const auto* assetManager = ServiceLocator::Get<AssetManager>();
					const auto resolution =
						assetManager && runtimeContext->defaultUiDocument.has_value() ?
						assetManager->GetContentPathResolver().ResolveFile(
							*runtimeContext->defaultUiDocument
						) :
						std::nullopt;
					if (resolution.has_value()) {
						const std::string defaultUiPath =
							resolution->resolvedPath.ToGenericUtf8();
						std::snprintf(
							context.pathBuffer.data(),
							context.pathBuffer.size(),
							"%s",
							defaultUiPath.c_str()
						);
					}
				}
			}
			context.previewZoom = std::clamp(context.previewZoom, 0.25f, 4.0f);
		}

		UiWidget* GetEditableDocumentRoot(UiRoot& uiRoot) {
			const UiWidget* runtimeRoot = uiRoot.GetRootWidget();
			if (!runtimeRoot) {
				return nullptr;
			}

			for (UiWidget* child : runtimeRoot->GetReferenceChildren()) {
				if (child) {
					return child;
				}
			}

			for (const auto& child : runtimeRoot->GetChildren()) {
				if (child) {
					return child.get();
				}
			}

			return nullptr;
		}

		bool IsDescendant(const UiWidget* ancestor, const UiWidget* node) {
			if (!ancestor || !node) {
				return false;
			}
			for (const auto& child : ancestor->GetChildren()) {
				if (!child) {
					continue;
				}
				if (child.get() == node || IsDescendant(child.get(), node)) {
					return true;
				}
			}
			for (const UiWidget* child : ancestor->GetReferenceChildren()) {
				if (!child) {
					continue;
				}
				if (child == node || IsDescendant(child, node)) {
					return true;
				}
			}
			return false;
		}

		bool ReparentWidget(UiWidget* widget, UiWidget* newParent) {
			if (!widget || !newParent || widget == newParent) {
				return false;
			}

			UiWidget* oldParent = widget->GetParent();
			if (!oldParent || IsDescendant(widget, newParent)) {
				return false;
			}

			std::unique_ptr<UiWidget> moved = oldParent->TakeChild(widget);
			if (!moved) {
				return false;
			}

			newParent->AddChild(std::move(moved));
			return true;
		}

		bool DeleteWidget(UiWidget* widget) {
			if (!widget) {
				return false;
			}
			UiWidget* parent = widget->GetParent();
			if (!parent) {
				return false;
			}

			const std::unique_ptr<UiWidget> removed = parent->TakeChild(widget);
			return removed != nullptr;
		}

		bool WidgetMatchesFilterRecursive(
			const UiWidget*    widget,
			const std::string& filterLower
		) {
			if (!widget || filterLower.empty()) {
				return true;
			}

			const std::string nameLower = StrUtil::ToLowerCase(
				widget->GetName()
			);
			if (nameLower.find(filterLower) != std::string::npos) {
				return true;
			}

			for (const auto& child : widget->GetChildren()) {
				if (WidgetMatchesFilterRecursive(child.get(), filterLower)) {
					return true;
				}
			}
			for (const UiWidget* refChild : widget->GetReferenceChildren()) {
				if (WidgetMatchesFilterRecursive(refChild, filterLower)) {
					return true;
				}
			}
			return false;
		}

		std::unique_ptr<UiWidget> CreateWidgetTemplate() {
			auto widget = std::make_unique<UiWidget>();
			widget->SetName("Widget");
			widget->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 0.0f, .maxY = 0.0f}
			);
			widget->SetPivot({.x = 0.0f, .y = 0.0f});
			widget->SetLocalRect(
				{.x = 0.0f, .y = 0.0f, .width = 160.0f, .height = 64.0f}
			);
			return widget;
		}

		std::unique_ptr<UiWidget> CreatePanelTemplate() {
			auto widget = CreateWidgetTemplate();
			widget->SetName("Panel");
			widget->SetLocalRect(
				{.x = 0.0f, .y = 0.0f, .width = 240.0f, .height = 120.0f}
			);
			const auto* panel = widget->GetOrAddComponent<
				UiPanelStyleComponent>();
			(void)panel;
			return widget;
		}

		std::unique_ptr<UiWidget> CreateButtonTemplate() {
			auto widget = CreateWidgetTemplate();
			widget->SetName("Button");
			widget->SetLocalRect(
				{.x = 0.0f, .y = 0.0f, .width = 180.0f, .height = 56.0f}
			);
			auto* behavior = widget->GetOrAddComponent<
				UiButtonBehaviorComponent>();
			behavior->SetText("Button");
			return widget;
		}

		std::unique_ptr<UiWidget> CreateVerticalLayoutTemplate() {
			auto widget = std::make_unique<UiWidget>();
			widget->SetName("VerticalLayout");
			widget->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 1.0f, .maxY = 1.0f}
			);
			widget->SetPivot({.x = 0.0f, .y = 0.0f});
			widget->SetLocalRect(
				{.x = 0.0f, .y = 0.0f, .width = 0.0f, .height = 0.0f}
			);
			auto* layout = widget->GetOrAddComponent<
				UiVerticalLayoutComponent>();
			layout->SetPadding(
				{.left = 8.0f, .top = 8.0f, .right = 8.0f, .bottom = 8.0f}
			);
			layout->SetSpacing(6.0f);
			return widget;
		}

		std::unique_ptr<UiWidget> CreateHorizontalLayoutTemplate() {
			auto widget = std::make_unique<UiWidget>();
			widget->SetName("HorizontalLayout");
			widget->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 1.0f, .maxY = 1.0f}
			);
			widget->SetPivot({.x = 0.0f, .y = 0.0f});
			widget->SetLocalRect(
				{.x = 0.0f, .y = 0.0f, .width = 0.0f, .height = 0.0f}
			);
			auto* layout = widget->GetOrAddComponent<
				UiHorizontalLayoutComponent>();
			layout->SetPadding(
				{.left = 8.0f, .top = 8.0f, .right = 8.0f, .bottom = 8.0f}
			);
			layout->SetSpacing(6.0f);
			return widget;
		}

		// パレットテンプレートはここで定義されます。
		constexpr std::array kPaletteTemplates = {
			PaletteTemplate{
				.category = "Basic", .label = "Widget",
				.tooltip  = "Empty widget with Transform only",
				.factory  = &CreateWidgetTemplate
			},
			PaletteTemplate{
				.category = "Basic", .label = "Panel",
				.tooltip  = "Widget + PanelStyle component",
				.factory  = &CreatePanelTemplate
			},
			PaletteTemplate{
				.category = "Controls", .label = "Button",
				.tooltip  = "Widget + ButtonBehavior component",
				.factory  = &CreateButtonTemplate
			},
			PaletteTemplate{
				.category = "Layout", .label = "Vertical Layout",
				.tooltip  = "Widget + VerticalLayout component",
				.factory  = &CreateVerticalLayoutTemplate
			},
			PaletteTemplate{
				.category = "Layout", .label = "Horizontal Layout",
				.tooltip  = "Widget + HorizontalLayout component",
				.factory  = &CreateHorizontalLayoutTemplate
			},
		};

		UiWidget* AddTemplateToParent(
			UiWidget&              parent,
			const PaletteTemplate& palette,
			GuiEditorContext&      context
		) {
			if (!palette.factory) {
				return nullptr;
			}

			parent.AddChild(palette.factory());
			const auto& children = parent.GetChildren();
			if (children.empty() || !children.back()) {
				return nullptr;
			}

			MarkDocumentDirty(context);
			return children.back().get();
		}

		bool DrawColor4(const char* label, Color& color) {
			float rgba[4] = {color.r, color.g, color.b, color.a};
			if (!ImGui::ColorEdit4(label, rgba)) {
				return false;
			}
			color.r = rgba[0];
			color.g = rgba[1];
			color.b = rgba[2];
			color.a = rgba[3];
			return true;
		}

		void DrawRenamePopup(GuiEditorContext& context) {
			if (!context.renameTargetWidget) {
				return;
			}

			if (ImGui::BeginPopupModal(
				kRenamePopupId,
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize
			)) {
				ImGui::TextUnformatted("Widget Name");
				ImGui::SetNextItemWidth(300.0f);
				ImGui::InputText(
					"##RenameWidgetText",
					context.renameBuffer.data(),
					context.renameBuffer.size()
				);

				if (ImGui::Button("Apply")) {
					context.renameTargetWidget->SetName(
						context.renameBuffer.data());
					MarkDocumentDirty(context);
					context.renameTargetWidget = nullptr;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					context.renameTargetWidget = nullptr;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		void DrawWidgetTreeNode(
			UiWidget*          editableRoot,
			UiWidget*          widget,
			GuiEditorContext&  context,
			const std::string& filterLower
		) {
			if (!widget || !WidgetMatchesFilterRecursive(widget, filterLower)) {
				return;
			}

			const bool         selected = widget == context.selectedWidget;
			ImGuiTreeNodeFlags flags    = ImGuiTreeNodeFlags_OpenOnArrow |
			                           ImGuiTreeNodeFlags_SpanAvailWidth;
			if (
				widget->GetChildren().empty() &&
				widget->GetReferenceChildren().empty()
			) {
				flags |=
					ImGuiTreeNodeFlags_Leaf |
					ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (selected) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			const std::string label = widget->GetName().empty() ?
				                          "(unnamed)" :
				                          std::string(widget->GetName());
			const bool open = ImGui::TreeNodeEx(
				widget,
				flags,
				"%s##%p",
				label.c_str(),
				widget
			);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				context.selectedWidget = widget;
			}

			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Rename")) {
					context.renameTargetWidget = widget;
					CopyStringToBuffer(
						widget->GetName(),
						context.renameBuffer.data(),
						context.renameBuffer.size()
					);
					ImGui::OpenPopup(kRenamePopupId);
				}

				const bool canDelete = widget != editableRoot &&
				                       widget->GetParent() != nullptr;
				if (!canDelete) {
					ImGui::BeginDisabled();
				}
				if (ImGui::MenuItem("Delete")) {
					context.pendingDeleteWidget = widget;
				}
				if (!canDelete) {
					ImGui::EndDisabled();
				}
				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource()) {
				UiWidget* payload = widget;
				ImGui::SetDragDropPayload(
					"UI_WIDGET_PTR",
					&payload,
					sizeof(payload)
				);
				ImGui::Text("Move: %s", label.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"UI_WIDGET_PTR"
				)) {
					UiWidget* dragged = *static_cast<UiWidget* const*>(payload->
						Data);
					if (ReparentWidget(dragged, widget)) {
						context.selectedWidget = dragged;
						MarkDocumentDirty(context);
					}
				}
				ImGui::EndDragDropTarget();
			}

			if (!open) {
				return;
			}

			for (const auto& child : widget->GetChildren()) {
				DrawWidgetTreeNode(
					editableRoot,
					child.get(),
					context,
					filterLower
				);
			}
			for (UiWidget* refChild : widget->GetReferenceChildren()) {
				DrawWidgetTreeNode(editableRoot, refChild, context,
				                   filterLower);
			}

			if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
				ImGui::TreePop();
			}
		}

		void ApplyPendingDeletion(
			UiWidget*         editableRoot,
			GuiEditorContext& context
		) {
			UiWidget* pendingDelete     = context.pendingDeleteWidget;
			context.pendingDeleteWidget = nullptr;
			if (!pendingDelete || pendingDelete == editableRoot) {
				return;
			}

			UiWidget* parent = pendingDelete->GetParent();
			if (!parent) {
				return;
			}

			if (context.selectedWidget == pendingDelete) {
				context.selectedWidget = parent;
			}
			if (context.renameTargetWidget == pendingDelete) {
				context.renameTargetWidget = nullptr;
			}

			if (DeleteWidget(pendingDelete)) {
				MarkDocumentDirty(context);
			}
		}

		void DrawWidgetBoundsRecursive(
			const UiWidget* widget,
			const ImVec2    imagePos,
			const float     scaleX,
			const float     scaleY,
			const UiWidget* selectedWidget
		) {
			if (!widget) {
				return;
			}

			const Rect  rect  = widget->GetGlobalRect();
			const ImU32 color = widget == selectedWidget ?
				                    IM_COL32(255, 210, 64, 255) :
				                    IM_COL32(64, 220, 255, 180);
			const float x0 = imagePos.x + rect.x * scaleX;
			const float y0 = imagePos.y + rect.y * scaleY;
			const float x1 = imagePos.x + (rect.x + rect.width) * scaleX;
			const float y1 = imagePos.y + (rect.y + rect.height) * scaleY;
			ImGui::GetWindowDrawList()->AddRect(
				ImVec2(x0, y0),
				ImVec2(x1, y1),
				color,
				0.0f,
				0,
				widget == selectedWidget ? 2.5f : 1.0f
			);

			for (const auto& child : widget->GetChildren()) {
				DrawWidgetBoundsRecursive(
					child.get(),
					imagePos,
					scaleX,
					scaleY,
					selectedWidget
				);
			}
			for (const UiWidget* refChild : widget->GetReferenceChildren()) {
				DrawWidgetBoundsRecursive(
					refChild,
					imagePos,
					scaleX,
					scaleY,
					selectedWidget
				);
			}
		}
	}

	void DrawUiHierarchyWindow(UiRoot& uiRoot, GuiEditorContext& context) {
		if (!ImGui::Begin("Ui Outliner")) {
			ImGui::End();
			return;
		}

		ImGui::InputTextWithHint(
			"##UiOutlinerFilter",
			"Search widgets",
			context.outlinerFilterBuffer.data(),
			context.outlinerFilterBuffer.size()
		);
		const std::string filterLower = StrUtil::ToLowerCase(
			context.outlinerFilterBuffer.data()
		);

		UiWidget* editableRoot = GetEditableDocumentRoot(uiRoot);
		if (editableRoot) {
			DrawWidgetTreeNode(editableRoot, editableRoot, context,
			                   filterLower);
		} else {
			ImGui::TextUnformatted(
				"No root widget. Load or create a document root.");
		}

		DrawRenamePopup(context);
		ApplyPendingDeletion(editableRoot, context);
		ImGui::End();
	}

	void DrawUiPaletteWindow(UiRoot& uiRoot, GuiEditorContext& context) {
		if (!ImGui::Begin("Ui Palette")) {
			ImGui::End();
			return;
		}

		UiWidget* editableRoot = GetEditableDocumentRoot(uiRoot);
		if (!editableRoot) {
			ImGui::TextUnformatted("No root widget. Palette is unavailable.");
			ImGui::End();
			return;
		}

		UiWidget* targetParent = context.selectedWidget ?
			                         context.selectedWidget :
			                         editableRoot;
		const std::string targetName = targetParent->GetName().empty() ?
			                               "(unnamed)" :
			                               std::string(targetParent->GetName());
		ImGui::Text("Add To: %s", targetName.c_str());
		ImGui::Separator();

		constexpr std::array<const char*, 3> categories = {
			"Basic", "Controls", "Layout"
		};
		for (const char* category : categories) {
			if (!ImGui::CollapsingHeader(category,
			                             ImGuiTreeNodeFlags_DefaultOpen)) {
				continue;
			}

			for (const auto& palette : kPaletteTemplates) {
				if (std::string_view(palette.category) != category) {
					continue;
				}

				if (ImGui::Button(palette.label, ImVec2(-1.0f, 0.0f))) {
					if (UiWidget* newWidget = AddTemplateToParent(
						*targetParent,
						palette,
						context
					)) {
						context.selectedWidget = newWidget;
					}
				}
				if (
					palette.tooltip &&
					palette.tooltip[0] != '\0' &&
					ImGui::IsItemHovered()
				) {
					ImGui::SetTooltip("%s", palette.tooltip);
				}
			}
		}

		ImGui::End();
	}

	void DrawUiInspectorWindow(
		GuiEditorContext& context, AssetManager& assetManager
	) {
		if (!ImGui::Begin("Ui Inspector")) {
			ImGui::End();
			return;
		}

		if (!context.selectedWidget) {
			ImGui::TextUnformatted("No widget selected.");
			ImGui::End();
			return;
		}

		UiWidget* selected = context.selectedWidget;
		bool      changed  = false;

		char nameBuffer[128] = {};
		CopyStringToBuffer(selected->GetName(), nameBuffer, sizeof(nameBuffer));
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
			selected->SetName(nameBuffer);
			changed = true;
		}

		bool visible = selected->IsVisible();
		if (ImGui::Checkbox("Visible", &visible)) {
			selected->SetVisible(visible);
			changed = true;
		}
		ImGui::SameLine();
		bool enabled = selected->IsEnabled();
		if (ImGui::Checkbox("Enabled", &enabled)) {
			selected->SetEnabled(enabled);
			changed = true;
		}

		Rect  rect         = selected->GetLocalRect();
		float rectArray[4] = {rect.x, rect.y, rect.width, rect.height};
		if (ImGui::DragFloat4("Local Rect", rectArray, 0.5f)) {
			selected->SetLocalRect(
				{
					.x     = rectArray[0], .y      = rectArray[1],
					.width = rectArray[2], .height = rectArray[3]
				}
			);
			changed = true;
		}

		Anchors anchors        = selected->GetAnchors();
		float   anchorArray[4] = {
			anchors.minX,
			anchors.minY,
			anchors.maxX,
			anchors.maxY
		};
		if (ImGui::SliderFloat4("Anchors", anchorArray, 0.0f, 1.0f)) {
			selected->SetAnchors(
				{
					.minX = anchorArray[0], .minY = anchorArray[1],
					.maxX = anchorArray[2], .maxY = anchorArray[3]
				}
			);
			changed = true;
		}

		Pivot pivot         = selected->GetPivot();
		float pivotArray[2] = {pivot.x, pivot.y};
		if (ImGui::SliderFloat2("Pivot", pivotArray, 0.0f, 1.0f)) {
			selected->SetPivot({.x = pivotArray[0], .y = pivotArray[1]});
			changed = true;
		}

		Margins margins        = selected->GetMargins();
		float   marginArray[4] = {
			margins.left,
			margins.top,
			margins.right,
			margins.bottom
		};
		if (ImGui::DragFloat4("Margins", marginArray, 0.5f)) {
			selected->SetMargins(
				{
					.left  = marginArray[0], .top    = marginArray[1],
					.right = marginArray[2], .bottom = marginArray[3]
				}
			);
			changed = true;
		}

		auto        sizePolicy       = selected->GetSizePolicy();
		const char* policies[]       = {"FIXED", "EXPAND"};
		int         horizontalPolicy = static_cast<int>(sizePolicy.horizontal);
		int         verticalPolicy   = static_cast<int>(sizePolicy.vertical);
		if (ImGui::Combo(
			"Horizontal Size Policy",
			&horizontalPolicy,
			policies,
			IM_ARRAYSIZE(policies)
		)) {
			sizePolicy.horizontal = static_cast<UiSizePolicyAxis>(
				horizontalPolicy
			);
			selected->SetSizePolicy(sizePolicy.horizontal, sizePolicy.vertical);
			changed = true;
		}
		if (ImGui::Combo(
			"Vertical Size Policy",
			&verticalPolicy,
			policies,
			IM_ARRAYSIZE(policies)
		)) {
			sizePolicy.vertical = static_cast<UiSizePolicyAxis>(verticalPolicy);
			selected->SetSizePolicy(sizePolicy.horizontal, sizePolicy.vertical);
			changed = true;
		}

		auto  constraints        = selected->GetSizeConstraints();
		float constraintArray[4] = {
			constraints.minWidth,
			constraints.minHeight,
			constraints.maxWidth,
			constraints.maxHeight
		};
		if (ImGui::DragFloat4("Size Constraints", constraintArray, 0.5f)) {
			selected->SetSizeConstraints(
				{
					.minWidth  = constraintArray[0],
					.minHeight = constraintArray[1],
					.maxWidth  = constraintArray[2],
					.maxHeight = constraintArray[3]
				}
			);
			changed = true;
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Components");

		int removeIndex   = -1;
		int moveFromIndex = -1;
		int moveToIndex   = -1;

		auto& components = selected->GetComponents();
		for (size_t i = 0; i < components.size(); ++i) {
			UiComponent* component = components[i].get();
			if (!component) {
				continue;
			}

			const std::string typeName(component->GetTypeName());
			ImGui::PushID(static_cast<int>(i));
			const bool canEditOrder = typeName != "Transform";

			if (ImGui::Button("^") && canEditOrder && i > 1) {
				moveFromIndex = static_cast<int>(i);
				moveToIndex   = static_cast<int>(i - 1);
			}
			ImGui::SameLine();
			if (
				ImGui::Button("v") &&
				canEditOrder &&
				i + 1 < components.size()
			) {
				moveFromIndex = static_cast<int>(i);
				moveToIndex   = static_cast<int>(i + 1);
			}
			ImGui::SameLine();
			if (ImGui::Button("X") && canEditOrder) {
				removeIndex = static_cast<int>(i);
			}
			ImGui::SameLine();

			const bool open = ImGui::TreeNodeEx(
				"##component",
				ImGuiTreeNodeFlags_DefaultOpen,
				"%s",
				typeName.c_str()
			);
			if (open) {
				if (auto* layout = dynamic_cast<UiLinearLayoutComponent*>(
					component)) {
					LayoutPadding padding         = layout->GetPadding();
					float         paddingArray[4] = {
						padding.left, padding.top, padding.right, padding.bottom
					};
					if (ImGui::DragFloat4("Padding", paddingArray, 0.5f)) {
						layout->SetPadding(
							{
								.left = paddingArray[0], .top = paddingArray[1],
								.right = paddingArray[2],
								.bottom = paddingArray[3]
							}
						);
						changed = true;
					}
					float spacing = layout->GetSpacing();
					if (ImGui::DragFloat("Spacing", &spacing, 0.1f)) {
						layout->SetSpacing(spacing);
						changed = true;
					}
				} else if (auto* panel = dynamic_cast<UiPanelStyleComponent*>(
					component)
				) {
					Color bg = panel->GetBackgroundColor();
					if (DrawColor4("Background", bg)) {
						panel->SetBackgroundColor(bg);
						changed = true;
					}
					float corner = panel->GetCornerRadius();
					if (ImGui::DragFloat(
						"Corner Radius",
						&corner,
						0.1f,
						0.0f,
						512.0f
					)) {
						panel->SetCornerRadius(corner);
						changed = true;
					}
					float borderThickness = panel->GetBorderThickness();
					if (ImGui::DragFloat(
						"Border Thickness",
						&borderThickness,
						0.1f,
						0.0f,
						64.0f
					)) {
						panel->SetBorderThickness(borderThickness);
						changed = true;
					}
					Color border = panel->GetBorderColor();
					if (DrawColor4("Border Color", border)) {
						panel->SetBorderColor(border);
						changed = true;
					}
				} else if (
					auto* button = dynamic_cast<UiButtonBehaviorComponent*>(
						component)
				) {
					char textBuffer[128] = {};
					CopyStringToBuffer(button->GetText(), textBuffer,
					                   sizeof(textBuffer));
					if (ImGui::InputText("Text", textBuffer,
					                     sizeof(textBuffer))) {
						button->SetText(textBuffer);
						changed = true;
					}

					Color normal  = button->GetNormalColor();
					Color hovered = button->GetHoveredColor();
					Color pressed = button->GetPressedColor();
					if (DrawColor4("Normal", normal)) {
						button->SetColors(normal, hovered, pressed);
						changed = true;
					}
					if (DrawColor4("Hovered", hovered)) {
						button->SetColors(normal, hovered, pressed);
						changed = true;
					}
					if (DrawColor4("Pressed", pressed)) {
						button->SetColors(normal, hovered, pressed);
						changed = true;
					}
					Color border = button->GetBorderColor();
					if (DrawColor4("Border", border)) {
						button->SetBorderColor(border);
						changed = true;
					}
					Color text = button->GetTextColor();
					if (DrawColor4("Text Color", text)) {
						button->SetTextColor(text);
						changed = true;
					}
					float corner = button->GetCornerRadius();
					if (ImGui::DragFloat(
						"Corner Radius",
						&corner,
						0.1f,
						0.0f,
						512.0f
					)) {
						button->SetCornerRadius(corner);
						changed = true;
					}
					float fontSize = button->GetFontSize();
					if (ImGui::DragFloat(
						"Font Size",
						&fontSize,
						0.1f,
						1.0f,
						128.0f
					)) {
						button->SetFontSize(fontSize);
						changed = true;
					}
				} else if (
					auto* texture = dynamic_cast<UiTextureComponent*>(component)
				) {
					std::string texturePath = texture->GetTexturePath().has_value() ?
						texture->GetTexturePath()->String() :
						std::string{};
					if (
						ImGuiWidgets::AssetPathPicker(
							"Texture",
							texturePath,
							ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::TEXTURE)
						)
					) {
						const auto virtualPath =
							VirtualPath::ParseContentReference(texturePath);
						if (!virtualPath.has_value()) {
							texture->ClearTexturePath();
						} else {
							(void)texture->SetTexturePath(
								*virtualPath, assetManager
							);
						}
						changed = true;
					}

					Color color = texture->GetColor();
					if (DrawColor4("Color", color)) {
						texture->SetColor(color);
						changed = true;
					}

					Vec2  uvMin         = texture->GetUvMin();
					float uvMinArray[2] = {uvMin.x, uvMin.y};
					if (ImGui::DragFloat2("UV Min", uvMinArray, 0.001f, 0.0f,
					                      1.0f)) {
						texture->SetUvMin(Vec2(uvMinArray[0], uvMinArray[1]));
						changed = true;
					}

					Vec2  uvMax         = texture->GetUvMax();
					float uvMaxArray[2] = {uvMax.x, uvMax.y};
					if (ImGui::DragFloat2("UV Max", uvMaxArray, 0.001f, 0.0f,
					                      1.0f)) {
						texture->SetUvMax(Vec2(uvMaxArray[0], uvMaxArray[1]));
						changed = true;
					}
				} else if (
					auto* strip = dynamic_cast<UiDigitStripComponent*>(
						component)
				) {
					std::string texturePath =
						strip->GetStripTexturePath().has_value() ?
						strip->GetStripTexturePath()->String() :
						std::string{};
					if (
						ImGuiWidgets::AssetPathPicker(
							"Strip Texture",
							texturePath,
							ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::TEXTURE)
						)
					) {
						const auto virtualPath =
							VirtualPath::ParseContentReference(texturePath);
						if (!virtualPath.has_value()) {
							strip->ClearStripTexturePath();
						} else {
							(void)strip->SetStripTexturePath(
								*virtualPath, assetManager
							);
						}
						changed = true;
					}

					int value = strip->GetValue();
					if (ImGui::DragInt("Value", &value, 1.0f, 0, 99999999)) {
						strip->SetValue(value);
						changed = true;
					}

					int minDigits = strip->GetMinDigits();
					if (ImGui::DragInt("Min Digits", &minDigits, 1.0f, 1, 16)) {
						strip->SetMinDigits(minDigits);
						changed = true;
					}

					float spacing = strip->GetDigitSpacing();
					if (ImGui::DragFloat("Digit Spacing", &spacing, 0.1f, 0.0f,
					                     256.0f)) {
						strip->SetDigitSpacing(spacing);
						changed = true;
					}

					Color color = strip->GetColor();
					if (DrawColor4("Color", color)) {
						strip->SetColor(color);
						changed = true;
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (moveFromIndex >= 0 && moveToIndex >= 0) {
			if (selected->MoveComponent(
				static_cast<size_t>(moveFromIndex),
				static_cast<size_t>(moveToIndex)
			)) {
				changed = true;
			}
		}
		if (removeIndex >= 0) {
			if (selected->RemoveComponentAt(static_cast<size_t>(removeIndex))) {
				changed = true;
			}
		}

		std::vector<std::string_view> componentTypes =
			UiWidget::GetRegisteredComponentTypeNames();
		std::vector<const char*> componentTypeLabels;
		componentTypeLabels.reserve(componentTypes.size());
		for (const auto typeName : componentTypes) {
			componentTypeLabels.emplace_back(typeName.data());
		}
		context.addComponentTypeIndex = std::clamp(
			context.addComponentTypeIndex,
			0,
			std::max(0, static_cast<int>(componentTypeLabels.size()) - 1)
		);
		if (!componentTypeLabels.empty()) {
			ImGui::Combo(
				"Add Component##GuiEditorAddComponentCombo",
				&context.addComponentTypeIndex,
				componentTypeLabels.data(),
				static_cast<int>(componentTypeLabels.size())
			);
			if (ImGui::Button("Add Component##GuiEditorAddComponentButton")) {
				const std::string_view typeName = componentTypes[static_cast<
					size_t>(
					context.addComponentTypeIndex
				)];
				if (
					typeName != "Transform" ||
					!selected->GetComponent<UiTransformComponent>()
				) {
					if (auto component = UiWidget::CreateComponentByTypeName(
						typeName)
					) {
						selected->AddComponent(std::move(component));
						changed = true;
					}
				}
			}
		}

		if (changed) {
			MarkDocumentDirty(context);
		}

		ImGui::End();
	}

	void DrawUiEditorMenu(
		UiDocumentManager&           manager,
		std::shared_ptr<UiDocument>& activeDocument,
		UiScreenStack*               screenStack,
		GuiEditorContext&            context
	) {
		EnsureContextDefaults(context);

		ImGui::Begin("UI Document");
		ImGui::InputText(
			"Path",
			context.pathBuffer.data(),
			context.pathBuffer.size()
		);

		const Path normalizedPath =
			Path(context.pathBuffer.data()).LexicallyNormal();

		if (ImGui::Button("Load")) {
			auto doc = manager.LoadDocument(normalizedPath);
			if (doc) {
				activeDocument             = doc;
				context.activeDocumentPath = normalizedPath;
				context.selectedWidget     = nullptr;
				context.documentChanged    = false;
				if (screenStack) {
					screenStack->Clear();
					screenStack->PushScreen(std::make_shared<UiScreen>(doc));
				}
				Msg(kChannelNone, "Document loaded: {}", normalizedPath);
			} else {
				Error(
					kChannelNone,
					"Failed to load document: {}",
					normalizedPath
				);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			if (manager.SaveDocument(normalizedPath, activeDocument)) {
				context.activeDocumentPath = normalizedPath;
				context.documentChanged    = false;
				Msg(kChannelNone, "Document saved: {}", normalizedPath);
			} else {
				Error(
					kChannelNone,
					"Failed to save document: {}",
					normalizedPath
				);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload From Asset")) {
			manager.ResolvePendingExternal(normalizedPath, true);
			if (auto doc = manager.LoadDocument(normalizedPath)) {
				activeDocument             = doc;
				context.activeDocumentPath = normalizedPath;
				context.selectedWidget     = nullptr;
				context.documentChanged    = false;
				if (screenStack) {
					screenStack->Clear();
					screenStack->PushScreen(std::make_shared<UiScreen>(doc));
				}
			}
		}

		const Path trackingPath = context.activeDocumentPath.IsEmpty() ?
			                          normalizedPath :
			                          context.activeDocumentPath;
		const bool dirty   = manager.IsDirty(trackingPath);
		const bool pending = manager.HasPendingExternal(trackingPath);

		ImGui::Separator();
		ImGui::Text("State: %s", dirty ? "Dirty" : "Clean");
		if (pending) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
				"External changes are pending."
			);
			if (ImGui::Button("Reload (Discard Local)")) {
				manager.ResolvePendingExternal(trackingPath, true);
				if (auto doc = manager.GetDocument(trackingPath)) {
					activeDocument         = doc;
					context.selectedWidget = nullptr;
					if (screenStack) {
						screenStack->Clear();
						screenStack->
							PushScreen(std::make_shared<UiScreen>(doc));
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Keep Local")) {
				manager.ResolvePendingExternal(trackingPath, false);
			}
		}

		if (activeDocument && !activeDocument->GetRootWidget()) {
			ImGui::Separator();
			ImGui::TextUnformatted("Document has no root.");
			if (ImGui::Button("Create Root (Vertical Layout)")) {
				auto root = CreateVerticalLayoutTemplate();
				root->SetName("Root");
				activeDocument->SetRootWidget(std::move(root));
				MarkDocumentDirty(context);
				if (screenStack) {
					screenStack->Clear();
					screenStack->PushScreen(
						std::make_shared<UiScreen>(activeDocument)
					);
				}
			}
		}

		ImGui::End();
	}

	void DrawUiPreviewWindow(
		UiRoot*                        uiRoot,
		const Render::SceneOutputView& previewOutput,
		const Vec2&                    previewSize,
		ImGuiLayer&                    imGuiLayer,
		GuiEditorContext&              context,
		const Vec2&                    canvasSize
	) {
		if (!ImGui::Begin("Ui Preview")) {
			ImGui::End();
			return;
		}

		if (!uiRoot) {
			ImGui::TextUnformatted("No runtime UI root.");
			ImGui::End();
			return;
		}

		context.previewZoom = std::clamp(context.previewZoom, 0.25f, 4.0f);
		if (ImGui::SmallButton(
			context.previewOverlayEnabled ?
				"Overlay: ON" :
				"Overlay: OFF"
		)) {
			context.previewOverlayEnabled = !context.previewOverlayEnabled;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("-")) {
			context.previewZoom = std::max(0.25f, context.previewZoom - 0.1f);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+")) {
			context.previewZoom = std::min(4.0f, context.previewZoom + 0.1f);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset")) {
			context.previewZoom = 1.0f;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Zoom", &context.previewZoom, 0.25f, 4.0f, "%.2fx");
		ImGui::Separator();

		const float targetWidth  = std::max(1.0f, canvasSize.x);
		const float targetHeight = std::max(1.0f, canvasSize.y);
		const float targetAspect = targetWidth / targetHeight;

		ImVec2 avail = ImGui::GetContentRegionAvail();
		avail.x      = std::max(1.0f, avail.x);
		avail.y      = std::max(1.0f, avail.y);

		float fitWidth  = avail.x;
		float fitHeight = fitWidth / targetAspect;
		if (fitHeight > avail.y) {
			fitHeight = avail.y;
			fitWidth  = fitHeight * targetAspect;
		}
		fitWidth  = std::max(1.0f, fitWidth);
		fitHeight = std::max(1.0f, fitHeight);

		const float drawWidth  = std::max(1.0f, fitWidth * context.previewZoom);
		const float drawHeight =
			std::max(1.0f, fitHeight * context.previewZoom);

		const ImVec2 baseCursor = ImGui::GetCursorScreenPos();
		const auto imagePos   = ImVec2(
			baseCursor.x + std::max(0.0f, (avail.x - drawWidth) * 0.5f),
			baseCursor.y + std::max(0.0f, (avail.y - drawHeight) * 0.5f)
		);
		ImGui::SetCursorScreenPos(imagePos);

		if (previewOutput.textureId != 0 && previewOutput.srvCpu.ptr != 0) {
			const ImTextureID textureId = imGuiLayer.GetOrCreateTextureId(
				previewOutput.textureId,
				previewOutput.srvRevision,
				previewOutput.srvCpu
			);
			ImGui::Image(textureId, ImVec2(drawWidth, drawHeight));
		} else {
			ImGui::InvisibleButton(
				"##UiPreviewPlaceholder",
				ImVec2(drawWidth, drawHeight)
			);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRect(
				imagePos,
				ImVec2(imagePos.x + drawWidth, imagePos.y + drawHeight),
				IM_COL32(130, 130, 130, 255),
				0.0f
			);
			drawList->AddText(
				ImVec2(imagePos.x + 8.0f, imagePos.y + 8.0f),
				IM_COL32(200, 200, 200, 255),
				"GUI Preview (RTT unavailable)"
			);
		}

		const bool   imageHovered = ImGui::IsItemHovered();
		const ImVec2 mousePos     = ImGui::GetMousePos();
		const float  u            = (mousePos.x - imagePos.x) / drawWidth;
		const float  v            = (mousePos.y - imagePos.y) / drawHeight;
		const bool   inside       = imageHovered &&
		                    u >= 0.0f &&
		                    u <= 1.0f &&
		                    v >= 0.0f &&
		                    v <= 1.0f;

		const float localX = inside ? u * targetWidth : -FLT_MAX;
		const float localY = inside ? v * targetHeight : -FLT_MAX;
		uiRoot->ProcessMouse(
			localX,
			localY,
			ImGui::IsMouseDown(ImGuiMouseButton_Left),
			ImGui::IsMouseClicked(ImGuiMouseButton_Left),
			ImGui::IsMouseReleased(ImGuiMouseButton_Left)
		);

		UiWidget* editableRoot = GetEditableDocumentRoot(*uiRoot);
		if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		    editableRoot) {
			if (UiWidget* hit = editableRoot->HitTest(localX, localY)) {
				context.selectedWidget = hit;
			}
		}

		if (context.previewOverlayEnabled && editableRoot) {
			const float scaleX = drawWidth / targetWidth;
			const float scaleY = drawHeight / targetHeight;
			DrawWidgetBoundsRecursive(
				editableRoot,
				imagePos,
				scaleX,
				scaleY,
				context.selectedWidget
			);
		}

		if (previewOutput.textureId == 0 && !context.textWarningLogged) {
			Warning(
				kChannelNone,
				"GUI preview output is unavailable yet. It will appear after one rendered frame."
			);
			context.textWarningLogged = true;
		}
		if (previewOutput.textureId != 0) {
			context.textWarningLogged = false;
		}

		const ImVec2 sourceSize = previewSize.x > 0.0f && previewSize.y > 0.0f ?
			                          ImVec2(previewSize.x, previewSize.y) :
			                          ImVec2(targetWidth, targetHeight);
		ImGui::Text("Preview RT: %.0f x %.0f", sourceSize.x, sourceSize.y);
		ImGui::Text("Canvas: %.0f x %.0f", targetWidth, targetHeight);
		ImGui::End();
	}
}
#endif
