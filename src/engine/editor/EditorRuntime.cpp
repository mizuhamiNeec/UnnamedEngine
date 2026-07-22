#ifdef _DEBUG
#include "EditorRuntime.h"

namespace Unnamed {
	EditorRuntime::EditorRuntime(
		ConsoleSystem*        console,
		InputSystem*          inputSystem,
		AssetManager*         assetManager,
		IDemoService*         demoService,
		IGameWorldFactory&    gameWorldFactory,
		Profiler*             profiler,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		ImGuiLayer&           imGuiLayer
	) : mConsole(console),
	    mToolHost(
		    console,
		    inputSystem,
		    assetManager,
		    demoService,
		    gameWorldFactory,
		    profiler,
		    windowManager,
		    renderModule,
		    imGuiLayer
	    ) {
		mToolHost.Initialize();
	}

	void EditorRuntime::BeginUI() const {
		mToolHost.BeginUI();
	}

	void EditorRuntime::BuildUi(const float deltaTime) {
		const EditorToolFrameContext frameContext = {
			.unscaledDeltaTime = deltaTime,
			.deltaTime         = deltaTime,
		};
		mToolHost.Tick(frameContext);
		mToolHost.BuildUi(frameContext);
	}

	void EditorRuntime::TogglePresentMode() const {
		mToolHost.TogglePresentMode();
	}

	EDITOR_PRESENT_MODE EditorRuntime::GetPresentMode() const {
		return mToolHost.GetPresentMode();
	}

	void EditorRuntime::FillEditorRenderViews(
		Render::RenderFrameInputs& inputs
	) const {
		mToolHost.CollectRenderViews(inputs);
	}

	void EditorRuntime::SyncViewOutputs() const {
		mToolHost.SyncViewOutputs();
	}

	void EditorRuntime::SyncPresentationState() const {
		mToolHost.SyncPresentationState();
	}

	World* EditorRuntime::GetRuntimeWorld() const {
		return mToolHost.GetRuntimeWorld();
	}
}

#endif
