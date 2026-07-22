#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/guidgenerator/GuidGenerator.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/sequence/SequenceRuntime.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/WorldCameraManager.h"
#include "engine/world/WorldDebugDraw.h"
#include "engine/physics/core/Physics.h"

namespace Unnamed {
	namespace Physics {
		class Engine;
	}

	class Scene;
	class AssetManager;
	class ConsoleSystem;
	class InputSystem;
	class Profiler;
	class AudioSystem;
	class IDemoService;
	class JsonReader;

	namespace Render {
		struct RenderFrameContext;
		struct RenderFrameInputs;
		struct RenderCameraInput;
	}

	struct WorldTime {
		float    fixedDeltaTime          = 0.0f;
		float    renderDeltaTime         = 0.0f;
		float    renderUnscaledDeltaTime = 0.0f;
		float    interpolationAlpha      = 0.0f;
		float    timeSeconds             = 0.0f;
		uint64_t fixedTickCounter        = 0;
	};

	/// @brief World が参照する外部サービス群です。
	/// @details ServiceLocator 依存を削減するために、Engine 側で構築して注入します。
	struct WorldServices {
		ConsoleSystem* console      = nullptr;
		InputSystem*   inputSystem  = nullptr;
		Profiler*      profiler     = nullptr;
		AssetManager*  assetManager = nullptr;
		IDemoService*  demoService  = nullptr;
		AudioSystem*   audioSystem  = nullptr;
	};

	class World {
	public:
		virtual ~World();

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] Scene& GetScene();

		/// @brief シーンを取得します。存在しない場合はnullptrを返します。
		[[nodiscard]] const Scene& GetScene() const;

		[[nodiscard]] Scene* GetScenePtr() noexcept;

		[[nodiscard]] const Scene* GetScenePtr() const noexcept;

		/// @brief 初期化します。
		virtual void Initialize();

		/// @brief シャットダウンします。
		virtual void Shutdown();

		/// @brief 互換目的のティック。内部的には FixedTick と RenderTick を順番に実行します。
		virtual void Tick(float unscaledDeltaTime, float deltaTime);

		/// @brief 固定シミュレーションティックを実行します。
		virtual void FixedTick(float fixedDeltaTime);

		/// @brief 固定シミュレーション前に、フレーム入力を反映します。
		virtual void FrameInputTick(float frameDeltaTime);

		/// @brief 描画フレームティックを実行します。
		virtual void RenderTick(
			float renderDeltaTime, float interpolationAlpha
		);

		/// @brief エディタ用のティックします。ゲームの時間スケールの影響を受けません。
		/// @param unscaledDeltaTime 前のフレームからの経過時間（秒）。ゲームの時間スケールの影響を受けません。
		virtual void EditorTick(float unscaledDeltaTime);

		/// @brief ファイルからシーンをロードします。
		/// @param path ロードするシーンのファイルパス
		/// @return ロードに成功した場合はtrue、失敗した場合はfalse
		bool LoadSceneFromFile(Path path);

		/// @brief 指定されたオプションでファイルからシーンをロードします。
		/// @param path ロードするシーンのファイルパス
		/// @param options シーン読込オプション
		/// @return ロードに成功した場合は true、失敗した場合は false
		virtual bool LoadSceneFromFile(
			Path path, const SceneLoadOptions& options
		);

		/// @brief シーンをファイルに保存します。
		/// @param path 保存するシーンのファイルパス
		/// @return 保存に成功した場合はtrue、失敗した場合はfalse
		virtual bool SaveSceneToFile(Path path) const;

		/// @brief 現在のシーンをアンロードします。
		virtual void UnloadScene();

		/// @brief シーン遷移を次の安全なタイミングで実行するようリクエストします。
		/// @param path 遷移先シーンのファイルパス
		void RequestSceneTransition(Path path);

		/// @brief 保留中のシーン遷移を処理します。
		/// @details エンジンのフレーム先頭など、安全なタイミングで呼び出してください。
		void ProcessPendingSceneTransition();

		/// @brief レンダリングフレームの入力を埋めます。
		virtual void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager,
			bool                        enableUiInput = true
		);

		/// @brief 現在のシーンが設定されるたびに増加する世代番号を返します。
		/// @details エンジンはこの番号を使い、新しいシーンをシミュレーション開始前にウォームアップします。
		[[nodiscard]] uint64_t GetSceneGeneration() const noexcept;

		/// @brief 実際にゲームシミュレーションを実行するワールドを返します。
		/// @details 通常は自身を返し、PIE のように更新を委譲するワールドは委譲先を返します。
		[[nodiscard]] virtual World* GetSimulationWorld() noexcept;

		/// @brief ゲームシミュレーションが有効かどうかを返します。デフォルトではtrueを返します。
		/// @return ゲームシミュレーションが有効な場合はtrue、そうでない場合はfalse
		[[nodiscard]] virtual bool IsGameSimulationEnabled() const noexcept;

		/// @brief ロードされたシーンのファイルパスを取得します。存在しない場合は空文字列を返します。
		/// @return ロードされたシーンのファイルパス
		[[nodiscard]] Path GetLoadedScenePath() const;

		/// @brief ロードされたシーンのファイルパスを設定します。
		/// @param path ロードされたシーンのファイルパス
		void SetLoadedScenePath(const Path& path);

		/// @brief ワールドの現在カメラマネージャを取得します。
		/// @return ワールドの現在カメラマネージャへの参照
		[[nodiscard]] WorldCameraManager& GetCameraManager() noexcept;

		/// @brief ワールドの現在カメラマネージャを取得します（const版）。
		/// @return ワールドの現在カメラマネージャへのconst参照
		[[nodiscard]] const WorldCameraManager&
		GetCameraManager() const noexcept;

		[[nodiscard]] GameplayCueBus&       GetGameplayCueBus() noexcept;
		[[nodiscard]] const GameplayCueBus& GetGameplayCueBus() const noexcept;

		/// @brief ワールドのシーケンスランタイムを取得します。
		[[nodiscard]] SequenceRuntime& GetSequenceRuntime() noexcept;

		/// @brief ワールドのシーケンスランタイムを取得します（const版）。
		[[nodiscard]] const SequenceRuntime&
		GetSequenceRuntime() const noexcept;

		/// @brief シーンを設定します。既にシーンが存在する場合はアンロードされます。
		/// @param scene 設定するシーンのユニークポインタ
		virtual void SetScene(std::unique_ptr<Scene> scene);

		[[nodiscard]] Physics::Engine& GetPhysicsEngine();

		[[nodiscard]] const Physics::Engine& GetPhysicsEngine() const;

		/// @brief 指定したポストFXパスの有効/無効を上書きします。
		/// @param passName ポストFXパス名（例: "Vignette"）
		/// @param enabled trueなら有効、falseなら無効
		void SetPostFxPassEnabledOverride(
			std::string_view passName, bool enabled
		);

		/// @brief 指定したポストFXパスの有効/無効上書きを解除します。
		/// @param passName ポストFXパス名
		void ClearPostFxPassEnabledOverride(std::string_view passName);

		/// @brief すべてのポストFXパス有効/無効上書きを解除します。
		void ClearPostFxPassEnabledOverrides();

		/// @brief 指定したポストFXパスのスカラー値を上書きします。
		/// @param passName ポストFXパス名
		/// @param paramName パラメータ名（例: "Intensity"）
		/// @param value スカラー値
		void SetPostFxScalarOverride(
			std::string_view passName, std::string_view paramName, float value
		);

		/// @brief 指定したポストFXパスの色値を上書きします。
		/// @param passName ポストFXパス名
		/// @param paramName パラメータ名（例: "Tint"）
		/// @param value 色値
		void SetPostFxColorOverride(
			std::string_view passName, std::string_view paramName,
			const Vec4&      value
		);

		/// @brief 指定したポストFXパスのパラメータ上書きを解除します。
		void ClearPostFxParamOverride(
			std::string_view passName, std::string_view paramName
		);

		/// @brief 指定したポストFXパスの全上書きを解除します。
		void ClearPostFxPassOverrides(std::string_view passName);

		/// @brief すべてのポストFXパス上書きを解除します。
		void ClearAllPostFxOverrides();

		/// @brief ワールド共通のデバッグ描画コンテキストを取得します。
		[[nodiscard]] WorldDebugDraw& GetDebugDraw() noexcept;

		/// @brief ワールド共通のデバッグ描画コンテキストを取得します（const版）。
		[[nodiscard]] const WorldDebugDraw& GetDebugDraw() const noexcept;

		/// @brief 画面空間デバッグスプライトを1件キューに積みます。
		void QueueDebugScreenSprite(const Render::ScreenSpriteInput& sprite);
		void QueueDebugScreenSprite(Render::ScreenSpriteInput&& sprite);

		[[nodiscard]] const WorldTime& GetTime() const noexcept;

		/// @brief World が利用するサービス参照を設定します。
		/// @param services 注入するサービス群
		void SetServices(const WorldServices& services) noexcept;

		/// @brief この World の既定シーン読込オプションを設定します。
		void SetSceneLoadOptions(const SceneLoadOptions& options) noexcept;

		/// @brief World が利用するサービス参照を取得します。
		/// @return 現在設定されているサービス群
		[[nodiscard]] const WorldServices& GetServices() const noexcept;

		/// @brief ConsoleSystem 参照を取得します。
		[[nodiscard]] ConsoleSystem* GetConsoleSystem() const noexcept;

		/// @brief InputSystem 参照を取得します。
		[[nodiscard]] InputSystem* GetInputSystem() const noexcept;

		/// @brief Profiler 参照を取得します。
		[[nodiscard]] Profiler* GetProfiler() const noexcept;

		/// @brief AssetManager 参照を取得します。
		[[nodiscard]] AssetManager* GetAssetManager() const noexcept;

		/// @brief DemoService 参照を取得します。
		[[nodiscard]] static IDemoService* GetDemoService() noexcept;

		/// @brief AudioSystem 参照を取得します。
		[[nodiscard]] AudioSystem* GetAudioSystem() const noexcept;

	protected:
		struct PostFxPassOverrides {
			std::unordered_map<std::string, Vec4>  colorParams;
			std::unordered_map<std::string, float> scalarParams;
			std::optional<bool>                    enabled;
		};

		[[nodiscard]] virtual bool BuildUiInputCamera(
			Render::RenderCameraInput& outCamera
		) const;

		/// @brief シーンがロードされたときに呼び出されます。派生クラスでオーバーライドして、シーンロード時の処理を実装できます。
		virtual void OnSceneLoaded();

		/// @brief シーンがアンロードされる前に呼び出されます。派生クラスでオーバーライドして、シーンアンロード時の処理を実装できます。
		virtual void OnSceneUnloaded();

		std::unordered_map<std::string, PostFxPassOverrides>
		mPostFxPassOverrides;

		std::unique_ptr<Scene>           mScene;         // 現在のシーン
		std::unique_ptr<Physics::Engine> mPhysicsEngine; // 物理エンジン
		WorldCameraManager               mCameraManager; // ワールドの現在カメラ管理
		GameplayCueBus                   mGameplayCueBus;
		std::unique_ptr<SequenceRuntime> mSequenceRuntime;

		GuidGenerator mGuidGenerator; // GUIDジェネレーター

		Path mLoadedScenePath;            // ロードされたシーンのファイルパス
		Path mPendingSceneTransitionPath; // 保留中のシーン遷移先
		uint64_t mSceneGeneration = 0;

		WorldTime      mTime; // ワールドの時間情報
		WorldDebugDraw mDebugDraw;

		std::vector<Render::ScreenSpriteInput> mDebugScreenSprites;

		WorldServices mServices;
		SceneLoadOptions mSceneLoadOptions = {};
	};
}
