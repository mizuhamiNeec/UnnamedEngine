#pragma once

#include <d3d12.h>
#include <memory>

#include <EntitySystem/EditorEntitySystem.h>

class SceneManager {
public:
	void Init();
	void Update(float deltaTime);
	void Render(ID3D12GraphicsCommandList* commandList);

	void SetMode(bool isEditorMode);

private:
	std::unique_ptr<EntitySystem> entitySystem_;
};

