#pragma once
#include <Editor.h>
#include <memory>
#include <SrvManager.h>

#include <CopyImagePass/CopyImagePass.h>

#include <ImGuiManager/ImGuiManager.h>

#include <Lib/Timer/EngineTimer.h>

#include <Line/LineCommon.h>

#include <Model/ModelCommon.h>

#include <ResourceSystem/Manager/ResourceManager.h>

#include <Scene/GameScene.h>
#include <Scene/Base/BaseScene.h>

#include <SceneManager/SceneFactory.h>
#include <SceneManager/SceneManager.h>

#include <Window/WindowManager.h>

class Console;
class D3D12;
class MainWindow;

class Engine {
public:
	Engine();
	void Run();

	[[nodiscard]] static bool IsEditorMode() {
		return mIsEditorMode;
	}

	[[nodiscard]] static D3D12* GetRenderer() {
		return mRenderer.get();
	}

	[[nodiscard]] static ResourceManager* GetResourceManager() {
		return mResourceManager.get();
	}

	[[nodiscard]] static ParticleManager* GetParticleManager() {
		return mParticleManager.get();
	}

	[[nodiscard]] static SrvManager* GetSrvManager() {
		return mSrvManager.get();
	}

	[[nodiscard]] static SceneManager* GetSceneManager() {
		return mSceneManager.get();
	}

	// シーン管理
	static void                       ChangeScene(const std::string& sceneName);
	static std::shared_ptr<BaseScene> GetCurrentScene();
	static void                       ToggleScenes(); // シーンの切り替え

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
	static void RegisterSceneCommands(); // シーン切り替えコマンド登録

	void CheckEditorMode();

	std::unique_ptr<WindowManager> mWindowManager;

	static std::unique_ptr<SrvManager>      mSrvManager;
	static std::unique_ptr<ResourceManager> mResourceManager;

	std::unique_ptr<EngineTimer> mTime;

	static std::unique_ptr<D3D12> mRenderer;
#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> mImGuiManager;
#endif
	static std::unique_ptr<ParticleManager> mParticleManager;

	std::unique_ptr<CopyImagePass> mCopyImagePass;

	std::unique_ptr<SpriteCommon>   mSpriteCommon;
	std::unique_ptr<Object3DCommon> mObject3DCommon;
	std::unique_ptr<ModelCommon>    mModelCommon;
	std::unique_ptr<LineCommon>     mLineCommon;

	std::unique_ptr<SceneFactory>        mSceneFactory;
	static std::shared_ptr<SceneManager> mSceneManager;

	std::unique_ptr<Editor> mEditor; // エディター

	std::unique_ptr<Console> mConsole;

	std::optional<std::string> mLoadFilePath; // シーンロードキュー

	std::unique_ptr<EntityLoader> mEntityLoader;

	RenderTargetTexture mOffscreenRtv;
	DepthStencilTexture mOffscreenDsv;
	RenderPassTargets   mOffscreenRenderPassTargets;

	RenderTargetTexture mPostProcessedRtv;
	DepthStencilTexture mPostProcessedDsv;
	RenderPassTargets   mPostProcessedRenderPassTargets;

	static Vec2 mViewportLt;
	static Vec2 mViewportSize;

	static bool mIsEditorMode; // エディターモードか?

	static bool mWishShutdown; // 終了したいか?
};
