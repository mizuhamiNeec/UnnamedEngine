#pragma once

#include "World.h"

namespace Unnamed {
	class GameWorld final : public World {
	public:
		using World::LoadSceneFromFile;

		~GameWorld() override;

		void Initialize() override;
		void Shutdown() override;
		void FixedTick(float fixedDeltaTime) override;
		void RenderTick(float renderDeltaTime, float interpolationAlpha) override;

		bool LoadSceneFromFile(
			Path path, const SceneLoadOptions& options
		) override;
		void UnloadScene() override;

		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager,
			bool                        enableUiInput = true
		) override;

		void SetScene(std::unique_ptr<Scene> scene) override;
	};
}
