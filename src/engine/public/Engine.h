#pragma once
#include <memory>
#include <string>
#include <vector>

#include <editor/public/Editor.h>

#include <engine/public/CopyImagePass/CopyImagePass.h>
#include <engine/public/Entity/EntityLoader.h>
#include <engine/public/ImGui/ImGuiManager.h>
#include <engine/public/Line/LineCommon.h>
#include <engine/public/Model/ModelCommon.h>
#include <engine/public/Object3D/Object3DCommon.h>
#include <engine/public/OldConsole/Console.h>
#include <engine/public/particle/ParticleManager.h>
#include <engine/public/postprocess/IPostProcess.h>
#include <engine/public/renderer/D3D12.h>
#include <engine/public/ResourceSystem/Manager/ResourceManager.h>
#include <engine/public/SceneManager/SceneFactory.h>
#include <engine/public/SceneManager/SceneManager.h>
#include <engine/public/Sprite/SpriteCommon.h>
#include <engine/public/subsystem/interface/ISubsystem.h>
#include <engine/public/Window/WindowManager.h>

#include <game/public/scene/base/BaseScene.h>

class TimeSystem;

namespace Unnamed {
	class ConsoleSystem;

	class Engine {
	public:
		Engine();
		~Engine();

		bool Init();
		void Update();
		void Shutdown() const;

		//---------------------------------------------------------------------
		// Purpose: 新エンジンクラス
		//---------------------------------------------------------------------
	private:
		std::vector<std::unique_ptr<ISubsystem>> mSubsystems;
		ConsoleSystem*                           mConsoleSystem = nullptr;
		TimeSystem*                              mTimeSystem    = nullptr;

	public:
		//---------------------------------------------------------------------
		// Purpose: 旧エンジンクラス
		//---------------------------------------------------------------------

		// DEPRECATED: 旧エンジンクラス
		static bool IsEditorMode() {
			return mIsEditorMode;
		}

		// DEPRECATED: 旧エンジンクラス
		static D3D12* GetRenderer() {
			return mRenderer.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static ResourceManager* GetResourceManager() {
			return mResourceManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static ParticleManager* GetParticleManager() {
			return mParticleManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static SrvManager* GetSrvManager() {
			return mSrvManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static SceneManager* GetSceneManager() {
			return mSceneManager.get();
		}

		// TODO: エンジンでやるべきじゃない
		// DEPRECATED: 旧エンジンクラス
		static void ChangeScene(const std::string& sceneName) {
			if (GetSceneManager() && sceneName != "") {
				GetSceneManager()->ChangeScene(sceneName);
			}
		}

		// DEPRECATED: 旧エンジンクラス
		static std::shared_ptr<BaseScene> GetCurrentScene();

		// DEPRECATED: 旧エンジンクラス
		static Vec2 GetViewportLT() {
			return mViewportLT;
		}

		// DEPRECATED: 旧エンジンクラス
		static Vec2 GetViewportSize() {
			return mViewportSize;
		}

		// DEPRECATED: 旧エンジンクラス
		static float blurStrength;


		void OnResize(uint32_t width, uint32_t height);
		void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);


		static void RegisterConsoleCommandsAndVariables();
		static void Quit(const std::vector<std::string>& args = {});
		static void SetEditorMode(const std::vector<std::string>& args = {});
		void        CheckEditorMode();

	private:
		std::unique_ptr<OldWindowManager> mWindowManager;

		static std::unique_ptr<SrvManager>      mSrvManager;
		static std::unique_ptr<ResourceManager> mResourceManager;

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

		std::unique_ptr<SceneFactory> mSceneFactory;

		static std::shared_ptr<SceneManager> mSceneManager;

		std::unique_ptr<Editor>  mEditor;
		std::unique_ptr<Console> mConsole;

		std::optional<std::string> mLoadFilePath;

		std::unique_ptr<EntityLoader> mEntityLoader;

		RenderTargetTexture                        mPingRtv[2];
		uint32_t                                   mPingIndex = 0;
		std::vector<std::unique_ptr<IPostProcess>> mPostChain;
		bool                                       bSwapchainPassBegun = false;

		RenderTargetTexture mOffscreenRtv;
		DepthStencilTexture mOffscreenDsv;
		RenderPassTargets   mOffscreenRenderPassTargets;

		RenderTargetTexture mPostProcessedRtv;
		DepthStencilTexture mPostProcessedDsv;
		RenderPassTargets   mPostProcessedRenderPassTargets;

		static Vec2 mViewportLT;
		static Vec2 mViewportSize;
		static bool mIsEditorMode;
		static bool mWishShutdown;
	};
}
