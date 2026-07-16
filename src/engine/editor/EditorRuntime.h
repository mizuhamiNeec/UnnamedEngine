#pragma once
#ifdef _DEBUG

#include "EditorProperties.h"
#include "EditorToolHost.h"

namespace Unnamed {
	class WindowManager;
	class ImGuiLayer;
	class World;
	class InputSystem;
	class AssetManager;
	class IDemoService;
	class IGameWorldFactory;
	class Profiler;

	namespace Render {
		class RenderModule;
		struct RenderFrameInputs;
	}

	/// @brief エディタランタイム
	/// @details エディタのUIやツールを管理するランタイムクラスです。エディタの初期化、UIの構築、レンダリングビューの収集、プレゼンテーション状態の同期などを行います。
	class EditorRuntime final {
	public:
		/// @brief エディタランタイムを初期化します。
		/// @param console コンソールシステムへのポインタ
		/// @param inputSystem 入力システムへのポインタ
		/// @param assetManager アセットマネージャへのポインタ
		/// @param demoService デモサービスへのポインタ
		/// @param gameWorldFactory ゲームワールドファクトリへの参照
		/// @param profiler プロファイラへのポインタ
		/// @param windowManager ウィンドウマネージャへの参照
		/// @param renderModule レンダーモジュールへの参照
		/// @param imGuiLayer ImGuiレイヤーへの参照
		EditorRuntime(
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

		/// @brief エディタのUIを開始します。
		void BeginUI() const;

		/// @brief エディタのUIを構築します。
		void BuildUi(float deltaTime);

		/// @brief 表示モードを切り替えます。
		void TogglePresentMode() const;

		/// @brief 現在の表示モードを取得します。
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

		/// @brief エディタのレンダリングビューを収集します。
		/// @param inputs レンダリングフレームの入力情報
		void FillEditorRenderViews(Render::RenderFrameInputs& inputs) const;

		/// @brief レンダリングビューの出力を同期します。
		void SyncViewOutputs() const;

		/// @brief プレゼンテーション状態を同期します。
		void SyncPresentationState() const;

		/// @brief ランタイムワールドを取得します。
		/// @return ランタイムワールドへのポインタ
		[[nodiscard]] World* GetRuntimeWorld() const;

	private:
		ConsoleSystem* mConsole = nullptr;
		EditorToolHost mToolHost;
	};
}

#endif
