#pragma once
#include <memory>
#include <vector>

class CameraComponent;

class CameraManager {
public:
	static void AddCamera(const std::shared_ptr<CameraComponent>& camera);

	static void RemoveCamera(const std::shared_ptr<CameraComponent>& camera);

	static void SetActiveCamera(const std::shared_ptr<CameraComponent>& camera);

	static void SwitchToNextCamera();

	static void SwitchToPrevCamera();

	static void SetActiveCameraByIndex(size_t index);

	static size_t GetActiveCameraIndex();

	static std::shared_ptr<CameraComponent> GetActiveCamera();

	static void Update(float deltaTime);

private:
	static std::vector<std::shared_ptr<CameraComponent>> cameras_;
	static std::shared_ptr<CameraComponent> activeCamera_;
};
