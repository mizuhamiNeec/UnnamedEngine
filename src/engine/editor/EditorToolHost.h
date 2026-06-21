#pragma once
#pragma once
#ifdef _DEBUG

#include <memory>
#include <vector>

#include "IEditorTool.h"
#include "LevelEditorTool.h"

namespace Unnamed {
	class ConsoleSystem;
	class InputSystem;
	class AssetManager;
	class IDemoService;
	class IGameWorldFactory;
	class Profiler;
	class WindowManager;
	class ImGuiLayer;

	namespace Render {
		class RenderModule;
		struct RenderFrameInputs;
		struct SceneOutputView;
	}

	class GuiEditorTool;

	class EditorToolHost final {
	public:
		EditorToolHost(
			ConsoleSystem*        console,
			InputSystem*          inputSystem,
			AssetManager*         assetManager,
			IDemoService*         demoService,
			IGameWorldFactory&    gameWorldFactory,
			Profiler*             profiler,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			ImGuiLayer&           imGuiLayer
		);
		~EditorToolHost();

		void Initialize();
		void Shutdown();
		void BeginUI() const;
		void Tick(const EditorToolFrameContext& frameContext) const;
		void BuildUi(const EditorToolFrameContext& frameContext);
		void CollectRenderViews(Render::RenderFrameInputs& inputs) const;
		void SyncViewOutputs() const;
		void SyncPresentationState() const;

		[[nodiscard]] World* GetRuntimeWorld() const;

		void                              TogglePresentMode() const;
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

	private:
		void RegisterTool(std::unique_ptr<IEditorTool> tool);

		WindowManager&        mWindowManager;
		Render::RenderModule& mRenderModule;
		ImGuiLayer&           mImGuiLayer;

		ConsoleSystem*     mConsole      = nullptr;
		InputSystem*       mInputSystem  = nullptr;
		AssetManager*      mAssetManager = nullptr;
		IDemoService*      mDemoService  = nullptr;
		IGameWorldFactory& mGameWorldFactory;
		Profiler*          mProfiler = nullptr;

		std::vector<std::unique_ptr<IEditorTool>> mOwnedTools;
		LevelEditorTool*                          mLevelTool = nullptr;

		std::unique_ptr<class EditorNotification>   mNotification;
		std::unique_ptr<class EditorLuaSystem>      mEditorLuaSystem;
		std::unique_ptr<class EditorGuiScriptPanel> mEditorGuiScriptPanel;

		bool mMainDockInitialized = false;
		bool mShowAbout           = false;
	};
}

#endif
