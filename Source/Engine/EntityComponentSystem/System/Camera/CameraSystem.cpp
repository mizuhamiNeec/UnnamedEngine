#include "CameraSystem.h"

#include "../EntityComponentSystem/Components/Camera/CameraComponent.h"
#include "../../../Window/Window.h"

void CameraSystem::RegisterComponent(CameraComponent* component) {
	if (std::ranges::find(components_, component) == components_.end()) {
		components_.push_back(component);
		if (!mainCamera_) {
			mainCamera_ = component;
		}
	}
}

void CameraSystem::UnregisterComponent(CameraComponent* component) {
	auto it = std::ranges::find(components_, component);
	if (it != components_.end()) {
		if (*it == mainCamera_) {
			mainCamera_ = components_.empty() ? nullptr : components_.front();
		}
		components_.erase(it);
	}
}

void CameraSystem::Initialize() {
	mainCamera_ = nullptr;
}

void CameraSystem::Update(float deltaTime) {
	for (CameraComponent* camera : components_) {
		camera->SetAspectRatio(static_cast<float>(Window::GetClientWidth()) / static_cast<float>(Window::GetClientHeight()));
		camera->Update(deltaTime);
	}
}

void CameraSystem::Terminate() {
}

void CameraSystem::SetMainCamera(CameraComponent* camera) {
	if (std::ranges::find(components_, camera) != components_.end()) {
		mainCamera_ = camera;
	}
}

CameraComponent* CameraSystem::GetMainCamera() const {
	return mainCamera_;
}
