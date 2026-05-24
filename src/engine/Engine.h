#pragma once
#include <functional>
#include <memory>

#include <core/assets/AssetID.h>

#include <engine/EngineConfig.h>

class IPostProcess;
class SrvManager;

namespace Unnamed {
	class Engine;
	class AssetManager;
	class AudioSystem;
	class ComponentRegistry;
	class EditorRuntime;
	class ImGuiLayer;
	class ConsoleSystem;
	class InputSystem;
	class Profiler;
	class WindowManager;
	class ConCommand;
	class IDemoService;
	class IGameWorldFactory;
	class World;
	struct GameRuntimeContext;
	struct WorldServices;

	namespace Render {
		class RenderModule;
		struct RenderFrameContext;
	}

	namespace Rhi {
		class IRhiDevice;
	}

	/// @brief Engine へ注入するゲームランタイム依存情報です。
	struct EngineRuntimeBindings {
		/// @brief スタンドアロン/PIE のワールド生成ファクトリです。
		IGameWorldFactory* gameWorldFactory = nullptr;
		/// @brief ゲームランタイムの起動コンテキストです。
		GameRuntimeContext* runtimeContext = nullptr;
		/// @brief Demo サービス生成関数です。
		std::function<std::unique_ptr<IDemoService>()> createDemoService = {};
	};

	/// @brief Engine 実行時フックです。
	struct EngineRunCallbacks {
		/// @brief Engine 初期化後に呼ばれます。false を返すと起動失敗として終了します。
		std::function<bool(Engine&)> onPostInitialize = {};
		/// @brief Engine シャットダウン直前に呼ばれます。
		std::function<void(Engine&)> onPreShutdown = {};
	};

	/// @brief エンジンクラス
	class Engine {
	public:
		/// @brief コンストラクタ
		/// @param runtimeBindings ゲームランタイム依存情報
		/// @param runMode 実行モード
		Engine(const EngineRuntimeBindings& runtimeBindings, RUN_MODE runMode);

		/// @brief デストラクタ
		~Engine();

		/// @brief エンジンの実行
		/// @param callbacks 実行時コールバック
		/// @return 終了コード
		int Run(const EngineRunCallbacks& callbacks = {});

		/// @brief エディターモードの画面表示モードを切り替えます。
		void ToggleEditorScreenMode() const;

	private:
		/// @brief 初期化処理
		bool Init();
		/// @brief 更新処理
		void Tick();
		/// @brief 終了処理
		void Shutdown();

		/// @brief コンソールコマンドとコンソール変数を登録します。
		void RegisterConsoleCommandsAndVariables();

		/// @brief エンジン標準コンポーネントの登録を保証します。
		/// @param componentRegistry 登録先のコンポーネントレジストリ
		static void RegisterEngineComponents(ComponentRegistry& componentRegistry);

		/// @brief シーン遷移処理の対象となるワールドを解決します。
		/// @param runtimeWorld 現在の実行対象ワールド
		/// @return 実際にシーン遷移を適用するワールド
		[[nodiscard]] static World* ResolveSceneTransitionTargetWorld(
			World* runtimeWorld
		);

		/// @brief フルスクリーンの切り替えを行います。
		void ToggleFullscreen() const;

		/// @brief ワールドを切り替えて初期化します。
		/// @param newWorld 切り替え先ワールド
		/// @return 切り替えたワールド参照
		World& ActivateWorld(std::unique_ptr<World> newWorld);

		/// @brief 現在状態から WorldServices を組み立てます。
		[[nodiscard]] WorldServices BuildWorldServices() const noexcept;

		/// @brief 現在のワールドを取得します。
		/// @return 現在のワールドの参照
		[[nodiscard]] World* GetWorld() const;

		EngineRuntimeBindings mRuntimeBindings = {};
		RUN_MODE     mRequestedRunMode = RUN_MODE::STANDALONE;
		EngineConfig mConfig;

		// 基本システム
		std::unique_ptr<AssetManager>             mAssetManager;
		std::unique_ptr<class PlatformEventsImpl> mPlatformEvents;
		std::unique_ptr<WindowManager>            mWindowManager;

		// 基幹システム
		std::unique_ptr<ConsoleSystem>         mConsoleSystem;
		std::unique_ptr<class TerminalSystem>  mTerminalSystem;
		std::unique_ptr<class TimeSystem>      mTimeSystem;
		std::unique_ptr<InputSystem>           mInputSystem;

		std::unique_ptr<Rhi::IRhiDevice>      mRhiDevice;
		std::unique_ptr<Render::RenderModule> mRenderModule;

		std::unique_ptr<Profiler>     mProfiler;
		std::unique_ptr<IDemoService> mDemoService;

		std::unique_ptr<World> mWorld;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		std::unique_ptr<ImGuiLayer>    mUImGuiLayer;
		std::unique_ptr<EditorRuntime> mUEditorRuntime;
#endif

		std::unique_ptr<AudioSystem> mAudioSystem;
		std::unique_ptr<ConCommand>  mQuitCommand;
		std::unique_ptr<ConCommand>  mToggleEditorCommand;
		std::unique_ptr<ConCommand>  mToggleFullscreenCommand;
		std::unique_ptr<ConCommand>  mMapCommand;
		std::unique_ptr<ConCommand>  mReloadSceneCommand;
		std::unique_ptr<ConCommand>  mPostFxSetCommand;
		std::unique_ptr<ConCommand>  mPostFxEnableCommand;
		std::unique_ptr<ConCommand>  mPostFxClearParamCommand;
		std::unique_ptr<ConCommand>  mPostFxClearPassCommand;
		std::unique_ptr<ConCommand>  mPostFxResetCommand;
		std::unique_ptr<ConCommand>  mPostFxListCommand;
		std::unique_ptr<ConCommand>  mPostFxChainCommand;
		std::unique_ptr<ConCommand>  mPostFxChainReloadCommand;
		std::unique_ptr<ConCommand>  mSequenceRegressionRunCommand;

		std::unique_ptr<Render::RenderFrameContext> mRenderFrameContext;
		float mAssetHotReloadPollAccumulator = 0.0f;
		float mSimulationAccumulator = 0.0f;
		uint32_t mFrameIndex = 0;
		uint32_t mLastResizeWidth = 0;
		uint32_t mLastResizeHeight = 0;
		uint32_t mLastLoggedTickRateMismatchConfigured = 0;

#ifdef _DEBUG
		bool mIsEditorMode = true;
#else
		bool mIsEditorMode = false;
#endif

		bool mWishShutdown = false;
	};
}
