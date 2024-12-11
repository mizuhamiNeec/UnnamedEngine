#pragma once

#include <vector>

#include "../Base/BaseSystem.h"

class CameraComponent;

class CameraSystem : public BaseSystem {
public:
	void RegisterComponent(CameraComponent* component);
	void UnregisterComponent(CameraComponent* component);

	// インターフェース
	void Initialize() override;
	void Update(float deltaTime) override;
	void Terminate() override;

	void SetMainCamera(CameraComponent* camera);
	void SwitchCamera(CameraComponent* newCamera);
	CameraComponent* GetMainCamera() const;

private:
	std::vector<CameraComponent*> components_;
	CameraComponent* mainCamera_ = nullptr;
};

