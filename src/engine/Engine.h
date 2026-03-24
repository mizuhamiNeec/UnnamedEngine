#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <editor/Editor.h>
#include <engine/EngineConfig.h>
#include <engine/CopyImagePass/CopyImagePass.h>
#include <engine/Entity/EntityLoader.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/Line/LineCommon.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Object3D/Object3DCommon.h>
#include <engine/OldConsole/Console.h>
#include <engine/particle/ParticleManager.h>
#include <engine/postprocess/IPostProcess.h>
#include <engine/postprocess/PostProcessPipeline.h>
#include <engine/renderer/D3D12.h>
#include <engine/renderer/RenderTargets.h>
#include <engine/ResourceSystem/Manager/ResourceManager.h>
#include <engine/SceneManager/SceneFactory.h>
#include <engine/SceneManager/SceneManager.h>
#include <engine/Sprite/SpriteCommon.h>
#include <engine/state/IEngineModeState.h>
#include <engine/unnamed/subsystem/input/UInputSystem.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/Window/WindowManager.h>
#include <game/scene/GameScene.h>

class AudioManager;

namespace Unnamed {
	/// @brief エンジンクラス
	class Engine {
	public:
		Engine();
		~Engine();

		int Run();

		[[nodiscard]] AudioManager* GetAudioManagerInstance() const { return mAudioManager.get(); }
		[[nodiscard]] D3D12* GetRendererInstance() const { return mRenderer.get(); }
		[[nodiscard]] ResourceManager* GetResourceManagerInstance() const { return mResourceManager.get(); }
		[[nodiscard]] SpriteCommon* GetSpriteCommonInstance() const { return mSpriteCommon.get(); }
		[[nodiscard]] ParticleManager* GetParticleManagerInstance() const { return mParticleManager.get(); }
		[[nodiscard]] SrvManager* GetSrvManagerInstance() const { return mSrvManager.get(); }
		[[nodiscard]] TexManager* GetTexManagerInstance() const {
			return mResourceManager ? mResourceManager->GetTexManager() : nullptr;
		}
		[[nodiscard]] SceneManager* GetSceneManagerInstance() const { return mSceneManager.get(); }
		[[nodiscard]] Vec2 GetViewportLTInstance() const { return mViewportLT; }
		[[nodiscard]] Vec2 GetViewportSizeInstance() const { return mViewportSize; }
		[[nodiscard]] float& GetBlurStrengthInstance() { return mBlurStrength; }

		void OnResize(uint32_t width, uint32_t height);
		void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);

		static void RegisterConsoleCommandsAndVariables();
		static void Quit(const std::vector<std::string>& args = {});
		void        CheckEditorMode();

		/// @brief シーン遷移をリクエストする（遅延実行される）
		static void RequestSceneChange(const std::string& name);

		/// @brief 現在シーンの再読込をリクエストする（遅延実行される）
		static void RequestCurrentSceneReload();

		/// @brief エディターインスタンスの取得
		Editor* GetEditor() const { return mEditor.get(); }

		/// @brief ポストプロセスエフェクト数の取得
		std::size_t GetPostChainSize() const {
			return mPostProcessPipeline.GetPassCount();
		}

		/// @brief ポストプロセスエフェクトの取得
		IPostProcess* GetPostProcessAt(int index) const {
			if (index < 0) { return nullptr; }
			return mPostProcessPipeline.GetPassAt(static_cast<size_t>(index));
		}

		/// @brief ビューポートをメインウィンドウ全体へ設定
		void SetViewportToMainWindow();

		/// @brief ビューポート(左上座標/サイズ)をエディター UI の値で更新
		void SetViewportFromEditor(float x, float y, float w, float h);

		/// @brief 現在の PingPong テクスチャの SRV (GPU ptr)
		uint64_t GetActivePingSrvGpuPtr() const {
			return mPostProcessPipeline.GetActivePingSrvGpuPtr();
		}

		/// @brief 現在の PingPong テクスチャの RTV リソース記述子
		D3D12_RESOURCE_DESC GetActivePingRtvDesc() const {
			return mPostProcessPipeline.GetActivePingRtvDesc();
		}

		/// @brief エディター生成
		void CreateEditor();

		/// @brief エディター破棄
		void DestroyEditor();

	private:
		/// @brief 初期化処理
		bool Init();
		/// @brief 更新処理
		void Tick();
		/// @brief 終了処理
		void Shutdown() const;

		/// @brief モード State を切り替える
		void SetModeState(std::unique_ptr<IEngineModeState> state);

		EngineConfig mConfig;

		std::unique_ptr<ConsoleSystem> mConsoleSystem;
		std::unique_ptr<TimeSystem>    mTimeSystem;
		std::unique_ptr<UInputSystem>  mInputSystem;

		std::unique_ptr<OldWindowManager> mWindowManager;
		std::unique_ptr<Editor> mEditor;
		std::unique_ptr<IEngineModeState> mModeState;
		std::unique_ptr<EntityLoader>     mEntityLoader;
		std::unique_ptr<Console>          mConsole;
		PostProcessPipeline               mPostProcessPipeline;
		RenderTargets                     mRenderTargets;
		bool                              bSwapchainPassBegun = false;

		static std::unique_ptr<SrvManager>      mSrvManager;
		static std::unique_ptr<ResourceManager> mResourceManager;

		static std::unique_ptr<D3D12> mRenderer;

#ifdef _DEBUG
		std::unique_ptr<ImGuiManager> mImGuiManager;
#endif

		static std::unique_ptr<ParticleManager> mParticleManager;

		static std::unique_ptr<SpriteCommon> mSpriteCommon;
		std::unique_ptr<Object3DCommon>      mObject3DCommon;
		std::unique_ptr<ModelCommon>         mModelCommon;
		std::unique_ptr<LineCommon>          mLineCommon;
		std::unique_ptr<SceneFactory>        mSceneFactory;
		static std::unique_ptr<AudioManager> mAudioManager;

		static std::shared_ptr<SceneManager> mSceneManager;

		static Vec2 mViewportLT;
		static Vec2 mViewportSize;
		static bool mIsEditorMode;
		static bool mWishShutdown;

		float mBlurStrength = 0.0f;
	};
}
