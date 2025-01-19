#include "CameraManager.h"

#include <Components/Camera/CameraComponent.h>

void CameraManager::AddCamera(const std::shared_ptr<CameraComponent>& camera) {
	cameras_.push_back(camera);
}

void CameraManager::RemoveCamera(const std::shared_ptr<CameraComponent>& camera) {
	std::erase(cameras_, camera);
}

void CameraManager::SetActiveCamera(const std::shared_ptr<CameraComponent>& camera) {
	activeCamera_ = camera;
}

std::shared_ptr<CameraComponent> CameraManager::GetActiveCamera() {
	return activeCamera_;
}

void CameraManager::Update(float deltaTime) {
	if (activeCamera_) {
		activeCamera_->Update(deltaTime);
	}
}

std::vector<std::shared_ptr<CameraComponent>> CameraManager::cameras_;
std::shared_ptr<CameraComponent> CameraManager::activeCamera_ = nullptr;
