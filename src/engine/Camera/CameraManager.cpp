#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>

void CameraManager::AddCamera(const std::shared_ptr<CameraComponent>& camera) {
	mCameras.emplace_back(camera);
}

void CameraManager::RemoveCamera(
	const std::shared_ptr<CameraComponent>& camera) {
	std::erase(mCameras, camera);
}

void CameraManager::SetActiveCamera(
	const std::shared_ptr<CameraComponent>& camera) {
	if (camera) {
		mActiveCamera = camera;
	} else {
		// カメラが存在しない場合は最初のカメラをアクティブにする
		if (!mCameras.empty()) {
			mActiveCamera = mCameras.front();
		}
	}
}

void CameraManager::SwitchToNextCamera() {
	if (mCameras.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t nextIndex    = (currentIndex + 1) % mCameras.size();
	SetActiveCameraByIndex(nextIndex);
}

void CameraManager::SwitchToPrevCamera() {
	if (mCameras.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t prevIndex    = currentIndex == 0
		                   ? mCameras.size() - 1
		                   : currentIndex - 1;
	SetActiveCameraByIndex(prevIndex);
}

void CameraManager::SetActiveCameraByIndex(const size_t index) {
	if (index < mCameras.size()) {
		SetActiveCamera(mCameras[index]);
	}
}

size_t CameraManager::GetActiveCameraIndex() {
	if (!mActiveCamera) {
		return 0;
	}
	auto it = std::ranges::find(mCameras, mActiveCamera);
	return it != mCameras.end() ? std::distance(mCameras.begin(), it) : 0;
}

std::shared_ptr<CameraComponent> CameraManager::GetActiveCamera() {
	return mActiveCamera;
}

void CameraManager::Update(const float deltaTime) {
	if (mActiveCamera) {
		if (mActiveCamera.use_count() > 1) {
			mActiveCamera->Update(deltaTime);
		}
	}
}

std::vector<std::shared_ptr<CameraComponent>> CameraManager::mCameras;
std::shared_ptr<CameraComponent> CameraManager::mActiveCamera = nullptr;
