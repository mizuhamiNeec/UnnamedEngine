#pragma once
#ifdef _DEBUG

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ContentBrowser.h"
#include "EditorViewportCameraManager.h"
#include "IEditorTool.h"

#include "core/math/Vec2.h"

#include "engine/Properties.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"

namespace Unnamed {
	class WindowManager;
	class EditorWorld;
	class ImGuiLayer;
	class ConsoleSystem;
	class InputSystem;
	class SequenceEditorController;
	class SequenceTimelinePanel;
	class SequenceCurvePanel;

	namespace Render {
		struct SceneOutputView;
	}

	enum class EDITOR_PRESENT_MODE : uint8_t {
		VIEWPORT_PANEL,
		FULLSCREEN_SWAP_CHAIN,
	};

	enum class EDITOR_VIEWPORT_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,
		FIXED_ASPECT_16_9,
		FIXED_ASPECT_4_3,
		HD720,
		FHD1080,
		UHD4K,
	};

	enum class EDITOR_GRID_SNAP_UNIT : uint8_t {
		METER = 0,
		HU    = 1,
	};

	/// @brief LevelEditorToolは、World hierarchy、viewport、component propertyを編集するlevel UIを提供します
	class LevelEditorTool final : public IEditorTool {
	public:
		LevelEditorTool(WindowManager& windowManager, ImGuiLayer& imGuiLayer);
		~LevelEditorTool() override = default;

		void Initialize(const EditorToolServices& services) override;
		void Shutdown() override;
		void BeginUI() override;
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
		[[nodiscard]] std::string_view GetToolId() const override;
		[[nodiscard]] std::string_view GetDisplayName() const override;
		[[nodiscard]] World*           GetRuntimeWorld() override;
		[[nodiscard]] bool             IsOpen() const override;
		void                           SetOpen(bool open) override;

		void                              TogglePresentMode();
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;
		/// @brief PresentModeに応じた入力ビューポート状態を同期します。
		void               SyncPresentationState();
		[[nodiscard]] bool IsPlaying() const;
		void               StartPlayInEditor() const;
		void               StopPlayInEditor() const;
		[[nodiscard]] bool IsProfilerWindowOpen() const;
		void               SetProfilerWindowOpen(bool open);

		static constexpr std::string_view kViewScenePerspective =
			"tool.level.scene.perspective";

	private:
		/// @brief ViewOutputCacheは、view textureのSRV、revision、texture ID、表示寸法とUV範囲を再利用用に保持します
		struct ViewOutputCache {
			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu      = {};
			uint64_t                    srvRevision = 0;
			uint32_t                    textureId   = 0;
			Vec2                        size        = Vec2::zero;
			Vec2                        uvMin       = Vec2(0.0f, 0.0f);
			Vec2                        uvMax       = Vec2(1.0f, 1.0f);
		};

		[[nodiscard]] Render::SceneViewRenderMode BuildSceneViewModeForSize(
			float width,
			float height,
			bool  forceFit = false
		) const;
		[[nodiscard]] Scene*       GetOutlinerScene();
		[[nodiscard]] const Scene* GetOutlinerScene() const;

		void        DrawMainMenu();
		static void DrawSideBar();
		void        DrawStatusBar();

		void DrawViewport(float deltaTime);
		void DrawViewportTopBar();
		void DrawViewportGizmo(
			std::string_view                   viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const Vec2&                        imagePos,
			float                              drawWidth,
			float                              drawHeight
		);
		void DrawViewportOverlay(float deltaTime) const;

		void DrawSceneOutliner();
		void DrawInspector() const;
		void DrawContentBrowser();
		void DrawProfilerWindow();

		[[nodiscard]] Entity* GetSelectedEntity() const;
		[[nodiscard]] bool    SaveSceneAs(Path path) const;

		/// @brief 指定パスのシーンをエディターワールドへロードします。
		/// @param path ロードするシーンパス
		/// @return ロードに成功した場合はtrue
		bool LoadSceneFromPath(Path path);

		[[nodiscard]] ViewportCameraBinding ResolveViewportBinding(
			std::string_view viewKey
		) const;

		std::unique_ptr<EditorWorld> mOwnedEditorWorld;
		EditorWorld&                 mEditorWorld;
		WindowManager&               mWindowManager;
		ImGuiLayer&                  mImGuiLayer;
		ConsoleSystem*               mConsoleSystem = nullptr;
		InputSystem*                 mInputSystem   = nullptr;

		EntityId mSelectedEntityId = 0;
		bool     mOpen             = true;
		bool     mInitialized      = false;
		bool     mDockInitialized  = false;

		EDITOR_PRESENT_MODE mPresentMode = EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
		EDITOR_VIEWPORT_RENDER_MODE mViewportRenderMode =
			EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
		std::string mActiveViewportViewKey = std::string(kViewScenePerspective);
		Vec2 mViewportPosition = Vec2::zero;
		Vec2 mViewportSize = Vec2::zero;
		Vec2 mLastViewportSize = Vec2(kClientWidth, kClientHeight);
		std::array<char, 512> mOpenScenePathBuffer = {};
		float mViewportPanelWidth = kClientWidth;
		float mViewportPanelHeight = kClientHeight;
		bool mViewportSizeChangedThisFrame = false;
		EditorViewportCameraManager mCameraManager = {};
		std::unordered_map<std::string, ViewOutputCache> mViewOutputs;

		float                 mGridSnap = 64.0f;
		EDITOR_GRID_SNAP_UNIT mGridSnapUnit = EDITOR_GRID_SNAP_UNIT::METER;
		float                 mAngleSnapDegree = 15.0f;
		float                 mCameraSpeedPopupTimer = 0.0f;
		bool                  mShowProfilerWindow = false;
		bool                  mViewportLookActive = false;

		EditorContentBrowser::BrowserViewState    mContentBrowserState = {};
		std::unique_ptr<SequenceEditorController> mSequenceEditorController;
	};
}

#endif
