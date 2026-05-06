#include "ParkourGameWorld.h"

#include <chrono>

#include "core/string/StrUtil.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	ParkourGameWorld::~ParkourGameWorld() = default;

	void ParkourGameWorld::Initialize() {
		const auto start = std::chrono::steady_clock::now();
		World::Initialize();
		const auto worldInitEnd = std::chrono::steady_clock::now();

		Msg(
			"ParkourGameWorld",
			"Initialize timing: worldInit={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				worldInitEnd - start
			).count()
		);
	}

	void ParkourGameWorld::Shutdown() {
		World::Shutdown();
	}

	void ParkourGameWorld::FixedTick(const float fixedDeltaTime) {
		World::FixedTick(fixedDeltaTime);
	}

	void ParkourGameWorld::RenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		World::RenderTick(renderDeltaTime, interpolationAlpha);
	}

	bool ParkourGameWorld::LoadSceneFromFile(const char* path) {
		const auto start = std::chrono::steady_clock::now();
		const bool ok    = [&] {
			if (!path || std::string_view(path).empty()) {
				return false;
			}

			const auto beforeUnload = std::chrono::steady_clock::now();
			UnloadScene();
			const auto afterUnload = std::chrono::steady_clock::now();

			// GUID が同一のシーン遷移でも競合しないよう旧シーン解除後にロードします。
			auto       newScene = std::make_unique<Scene>();
			newScene->SetWorld(this);
			const bool loadOk   = SceneSerializer::LoadFromFile(
				*newScene, path, mGuidGenerator
			);
			if (!loadOk) {
				return false;
			}

			World::SetScene(std::move(newScene));
			const auto afterSetScene = std::chrono::steady_clock::now();

			mLoadedScenePath = StrUtil::NormalizePath(path);
			OnSceneLoaded();

			Msg(
				"ParkourGameWorld",
				"LoadSceneFromFile timing: load={}ms unload={}ms setScene={}ms total={}ms path={}",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					beforeUnload - start
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterUnload - beforeUnload
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterSetScene - afterUnload
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterSetScene - start
				).count(),
				std::string(path)
			);
			return true;
		}();

		if (!ok) {
			const auto end = std::chrono::steady_clock::now();
			Msg(
				"ParkourGameWorld",
				"LoadSceneFromFile timing: failed total={}ms path={}",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					end - start
				).count(),
				path ? std::string(path) : std::string("<null>")
			);
		}
		return ok;
	}

	void ParkourGameWorld::UnloadScene() {
		if (!mScene) {
			return;
		}
		World::UnloadScene();
	}

	void ParkourGameWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		World::FillRenderFrameInputs(inputs, frameContext, assetManager);
	}

	void ParkourGameWorld::SetScene(std::unique_ptr<Scene> scene) {
		if (mScene) {
			UnloadScene();
		}
		World::SetScene(std::move(scene));
	}
}
