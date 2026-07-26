#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "IEditorTool.h"

#include "engine/gui/UiDocument.h"
#include "engine/gui/UiDocumentManager.h"
#include "engine/gui/UiDrawCommand.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiScreenStack.h"
#include "engine/gui/editor/GuiEditor.h"
#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class ImGuiLayer;

	namespace Render {
		struct SceneOutputView;
	}

	/// @brief GuiEditorToolは、GUI documentのnode選択、property編集、preview描画をEditor toolとして提供します
	class GuiEditorTool final : public IEditorTool {
	public:
		explicit GuiEditorTool(ImGuiLayer& imGuiLayer);
		~GuiEditorTool() override = default;

		void Initialize(const EditorToolServices& services) override;
		void Shutdown() override;
		void Tick(const EditorToolFrameContext& frameContext) override;
		void BuildUi(const EditorToolFrameContext& frameContext) override;
		void CollectRenderViews(Render::RenderFrameInputs& inputs) override;
		void EnumerateViewKeys(
			std::vector<std::string>& outViewKeys
		) const override;
		void SetViewOutput(
			std::string_view               viewKey,
			const Render::SceneOutputView& output,
			Vec2                           size
		) override;
		[[nodiscard]] bool IsOpen() const override;
		void               SetOpen(bool open) override;

		[[nodiscard]] std::string_view GetToolId() const override;
		[[nodiscard]] std::string_view GetDisplayName() const override;

		static constexpr std::string_view kViewGuiPreview =
			"tool.gui.preview";

	private:
		/// @brief GUI プレビュー出力の SRV と有効領域を UI 構築まで保持します。
		struct ViewOutputCache {
			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu      = {};
			uint64_t                    srvRevision = 0;
			uint32_t                    textureId   = 0;
			Vec2                        uvMin       = Vec2::zero;
			Vec2                        uvMax       = Vec2::one;
			Vec2                        size        = Vec2::zero;
		};

		ImGuiLayer& mImGuiLayer;
		AssetManager* mAssetManager = nullptr;

		bool mOpen = false;
		bool mInitialized = false;
		bool mGuiEditorDockInitialized = false;
		Vec2 mGuiPreviewResolution = Vec2(1920.0f, 1080.0f);
		std::unordered_map<std::string, ViewOutputCache> mViewOutputs;

		std::unique_ptr<Gui::UiDocumentManager> mGuiDocumentManager;
		std::shared_ptr<Gui::UiDocument> mGuiActiveDocument;
		std::unique_ptr<Gui::UiRoot> mGuiEditorRoot;
		std::unique_ptr<Gui::UiScreenStack> mGuiEditorScreenStack;
		std::unique_ptr<Gui::GuiEditorContext> mGuiEditorContext;
		std::vector<Gui::UiDrawCommand> mGuiPreviewDrawCommands;
		std::vector<Render::ScreenSpriteInput> mGuiPreviewSprites;
		bool mGuiPreviewTextWarningLogged = false;
	};
}

#endif
