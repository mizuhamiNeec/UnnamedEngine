#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>

/// @brief カメラを追加する
/// @param camera 追加するカメラコンポーネントの共有ポインタ
void CameraManager::AddCamera(const std::shared_ptr<CameraComponent>& camera) {
	mCameras.emplace_back(camera);
}

/// @brief カメラを削除する
/// @param camera 削除するカメラコンポーネントの共有ポインタ
void CameraManager::RemoveCamera(
	const std::shared_ptr<CameraComponent>& camera) {
	std::erase(mCameras, camera);
}

/// @brief アクティブなカメラを設定する
/// @param camera アクティブにするカメラコンポーネントの共有ポインタ
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

/// @brief 次のカメラに切り替える
/// @note カメラが複数存在する場合にのみ動作する
void CameraManager::SwitchToNextCamera() {
	if (mCameras.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t nextIndex    = (currentIndex + 1) % mCameras.size();
	SetActiveCameraByIndex(nextIndex);
}

/// @brief 前のカメラに切り替える
/// @note カメラが複数存在する場合にのみ動作する
void CameraManager::SwitchToPrevCamera() {
	if (mCameras.empty())
		return;
	size_t currentIndex = GetActiveCameraIndex();
	size_t prevIndex    = currentIndex == 0 ?
		                   mCameras.size() - 1 :
		                   currentIndex - 1;
	SetActiveCameraByIndex(prevIndex);
}

/// @brief インデックスでアクティブなカメラを設定する
/// @param index アクティブにするカメラのインデックス
void CameraManager::SetActiveCameraByIndex(const size_t index) {
	if (index < mCameras.size()) {
		SetActiveCamera(mCameras[index]);
	}
}

/// @brief アクティブなカメラのインデックスを取得する
/// @return アクティブなカメラのインデックス
size_t CameraManager::GetActiveCameraIndex() {
	if (!mActiveCamera) {
		return 0;
	}
	auto it = std::ranges::find(mCameras, mActiveCamera);
	return it != mCameras.end() ? std::distance(mCameras.begin(), it) : 0;
}

/// @brief アクティブなカメラを取得する
/// @return アクティブなカメラコンポーネントの共有ポインタ
std::shared_ptr<CameraComponent> CameraManager::GetActiveCamera() {
	return mActiveCamera;
}

/// @brief アクティブなカメラを更新する
/// @param deltaTime 前のフレームからの経過時間
void CameraManager::Update(const float deltaTime) {
	if (mActiveCamera) {
		if (mActiveCamera.use_count() > 1) {
			mActiveCamera->Update(deltaTime);
		}
	}
}

std::vector<std::shared_ptr<CameraComponent>> CameraManager::mCameras;
std::shared_ptr<CameraComponent> CameraManager::mActiveCamera = nullptr;
