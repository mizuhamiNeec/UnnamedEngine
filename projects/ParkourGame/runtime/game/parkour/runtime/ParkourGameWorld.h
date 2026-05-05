#pragma once

#include "engine/world/World.h"

namespace Unnamed {
	/// @brief Parkour ゲーム向けのランタイムワールド実装です。
	class ParkourGameWorld final : public World {
	public:
		/// @brief デストラクタです。
		~ParkourGameWorld() override;

		/// @brief ワールドを初期化します。
		void Initialize() override;
		/// @brief ワールドを終了処理します。
		void Shutdown() override;
		/// @brief 固定ティックを実行します。
		void FixedTick(float fixedDeltaTime) override;
		/// @brief 描画ティックを実行します。
		void RenderTick(float renderDeltaTime, float interpolationAlpha) override;

		/// @brief シーンファイルをロードします。
		bool LoadSceneFromFile(const char* path) override;
		/// @brief シーンをアンロードします。
		void UnloadScene() override;

		/// @brief レンダリング入力を構築します。
		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;

		/// @brief 現在のシーンを差し替えます。
		void SetScene(std::unique_ptr<Scene> scene) override;
	};
}
