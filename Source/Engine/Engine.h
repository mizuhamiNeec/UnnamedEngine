#pragma once
#include <Editor.h>
#include <memory>
#include <ImGuiManager/ImGuiManager.h>
#include <Lib/Timer/EngineTimer.h>
#include <Line/LineCommon.h>
#include <Model/ModelCommon.h>
#include <ResourceSystem/Manager/ResourceManager.h>
#include <Scene/Base/BaseScene.h>
#include <SceneManager/SceneFactory.h>
#include <SceneManager/SceneManager.h>
#include <Window/WindowManager.h>
#include <SrvManager.h>

#include "CopyImagePass/CopyImagePass.h"
#include "CubeMap/CubeMap.h"

class Console;
class D3D12;
class MainWindow;

class Engine {
public:
	Engine();
	void Run();

	[[nodiscard]] static bool IsEditorMode() {
		return bIsEditorMode_;
	}

	[[nodiscard]] static D3D12* GetRenderer() {
		return renderer_.get();
	}

	[[nodiscard]] static ResourceManager* GetResourceManager() {
		return resourceManager_.get();
	}

	[[nodiscard]] static ParticleManager* GetParticleManager() {
		return particleManager_.get();
	}

	void OnResize(uint32_t width, uint32_t height);
	void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);

	static Vec2 GetViewportLT();
	static Vec2 GetViewportSize();

private:
	void Init();
	void Update();
	void Shutdown() const;

	static void RegisterConsoleCommandsAndVariables();
	static void Quit(const std::vector<std::string>& args = {});

	void CheckEditorMode();

	std::unique_ptr<WindowManager> wm_;

	std::unique_ptr<SrvManager>             srvManager_;
	static std::unique_ptr<ResourceManager> resourceManager_;

	std::unique_ptr<EngineTimer> time_;

	static std::unique_ptr<D3D12> renderer_;
#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> imGuiManager_;
#endif
	static std::unique_ptr<ParticleManager> particleManager_;

	std::unique_ptr<CopyImagePass> copyImagePass_;

	std::unique_ptr<SpriteCommon>   spriteCommon_;
	std::unique_ptr<Object3DCommon> object3DCommon_;
	std::unique_ptr<ModelCommon>    modelCommon_;
	std::unique_ptr<LineCommon>     lineCommon_;

	std::unique_ptr<SceneFactory> sceneFactory_;
	std::shared_ptr<SceneManager> sceneManager_;

	std::unique_ptr<Editor> editor_; // エディター

	std::unique_ptr<Console> console_;

	RenderTargetTexture offscreenRTV_;
	DepthStencilTexture offscreenDSV_;
	RenderPassTargets   offscreenRenderPassTargets_;

	RenderTargetTexture postProcessedRTV_;
	DepthStencilTexture postProcessedDSV_;
	RenderPassTargets   postProcessedRenderPassTargets_;

	static Vec2 viewportLT;
	static Vec2 viewportSize;

	static bool bIsEditorMode_; // エディターモードか?

	static bool bWishShutdown_; // 終了したいか?
};
