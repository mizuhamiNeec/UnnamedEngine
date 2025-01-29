#pragma once
#include "Base/EntitySystem.h"

class EditorEntitySystem : public EntitySystem {
public:
	~EditorEntitySystem() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;

private:
	void RenderOutliner();
	void RenderInspector();
};

