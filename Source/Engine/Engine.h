#pragma once
#include <Editor.h>
#include <memory>
#include <Lib/Timer/EngineTimer.h>
#include <Line/LineCommon.h>
#include <Model/ModelCommon.h>
#include <Scene/Base/Scene.h>
#include <UnnamedResource/SRV/ShaderResourceViewManager.h>

#include "Components/MeshRenderer/StaticMeshRenderer.h"

#include "UnnamedResource/Manager/ResourceManager.h"
#include "UnnamedResource/Texture/Texture.h"

class Console;
class ImGuiManager;
class D3D12;
class Window;

class Engine {
public:
	Engine();
	void Run();

	void ChangeScene(const std::shared_ptr<Scene>& newScene);

	[[nodiscard]] static bool IsEditorMode() {
		return bIsEditorMode_;
	}

	[[nodiscard]] static D3D12* GetRenderer() {
		return renderer_.get();
	}

	[[nodiscard]] Window* GetWindow() const {
		return window_.get();
	}

	[[nodiscard]] Entity* GetCameraEntity() const {
		return cameraEntity_.get();
	}

	[[nodiscard]] CameraComponent* GetCameraComponent() const {
		return camera_.get();
	}

	[[nodiscard]] SpriteCommon* GetSpriteCommon() const {
		return spriteCommon_.get();
	}

	[[nodiscard]] ParticleManager* GetParticleManager() const {
		return particleManager_.get();
	}

	[[nodiscard]] Object3DCommon* GetObject3DCommon() const {
		return object3DCommon_.get();
	}

	[[nodiscard]] ModelCommon* GetModelCommon() const {
		return modelCommon_.get();
	}

	[[nodiscard]] LineCommon* GetLineCommon() const {
		return lineCommon_.get();
	}

	[[nodiscard]] EngineTimer* GetEngineTimer() const {
		return time_.get();
	}

	[[nodiscard]] ResourceManager* GetResourceManager() const {
		return resourceManager_.get();
	}

private:
	void Init();
	void Update();
	void Shutdown() const;

	static void RegisterConsoleCommandsAndVariables();
	static void Quit(const std::vector<std::string>& args = {});

	void CheckEditorMode();

	std::unique_ptr<Window> window_;
	static std::unique_ptr<D3D12> renderer_;

	std::unique_ptr<ResourceManager> resourceManager_;

	std::unique_ptr<EngineTimer> time_;

	std::unique_ptr<Entity> cameraEntity_;
	std::shared_ptr<CameraComponent> camera_;

	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<ParticleManager> particleManager_;
	std::unique_ptr<Object3DCommon> object3DCommon_;
	std::unique_ptr<ModelCommon> modelCommon_;
	std::unique_ptr<LineCommon> lineCommon_;

	std::shared_ptr<Texture> texture;

	std::shared_ptr<Scene> currentScene_; // 現在のシーン
	std::unique_ptr<Editor> editor_; // エディター
	static bool bIsEditorMode_; // エディターモードか?

	static bool bWishShutdown_; // 終了したいか?

	Vec3 rot_ = Vec3::zero; // TODO : 消す予定

#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
#endif
};
