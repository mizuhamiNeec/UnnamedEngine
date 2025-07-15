#include "CameraManager.h"

#include <Components/Camera/CameraComponent.h>

void CameraManager::AddCamera(const std::shared_ptr<CameraComponent>& camera) {
	cameras_.emplace_back(camera);
}

void CameraManager::RemoveCamera(
	const std::shared_ptr<CameraComponent>& camera) {
	std::erase(cameras_, camera);
}

void CameraManager::SetActiveCamera(
	const std::shared_ptr<CameraComponent>& camera) {
	if (camera) {
		activeCamera_ = camera;
	} else {
		// カメラが存在しない場合は最初のカメラをアクティブにする
		if (!cameras_.empty()) {
			activeCamera_ = cameras_.front();
		}
	}
}

void CameraManager::SwitchToNextCamera() {
	if (cameras_.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t nextIndex    = (currentIndex + 1) % cameras_.size();
	SetActiveCameraByIndex(nextIndex);
}

void CameraManager::SwitchToPrevCamera() {
	if (cameras_.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t prevIndex    = currentIndex == 0
		                      ? cameras_.size() - 1
		                      : currentIndex - 1;
	SetActiveCameraByIndex(prevIndex);
}

void CameraManager::SetActiveCameraByIndex(const size_t index) {
	if (index < cameras_.size()) {
		SetActiveCamera(cameras_[index]);
	}
}

size_t CameraManager::GetActiveCameraIndex() {
	if (!activeCamera_) {
		return 0;
	}
	auto it = std::ranges::find(cameras_, activeCamera_);
	return it != cameras_.end() ? std::distance(cameras_.begin(), it) : 0;
}

std::shared_ptr<CameraComponent> CameraManager::GetActiveCamera() {
	return activeCamera_;
}

void CameraManager::Update(const float deltaTime) {
	if (activeCamera_) {
		if (activeCamera_.use_count() > 1) {
			activeCamera_->Update(deltaTime);
		}
	}
}

std::vector<std::shared_ptr<CameraComponent>> CameraManager::cameras_;
std::shared_ptr<CameraComponent> CameraManager::activeCamera_ = nullptr;
