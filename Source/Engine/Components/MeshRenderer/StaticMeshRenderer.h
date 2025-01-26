#pragma once
#include <Components/MeshRenderer/Base/MeshRenderer.h>

#include <ResourceSystem/Mesh/StaticMesh.h>

#include "Entity/Base/Entity.h"

#include "Renderer/ConstantBuffer.h"

class StaticMeshRenderer : public MeshRenderer {
public:
	StaticMeshRenderer() = default;
	virtual ~StaticMeshRenderer();

	void OnAttach(Entity& owner) override;

	// 描画処理
	virtual void Render(ID3D12GraphicsCommandList* commandList) override;
	// インスペクターの描画
	virtual void DrawInspectorImGui() override;

	StaticMesh* GetStaticMesh() const;
	void SetStaticMesh(StaticMesh* staticMesh);

protected:
	void BindTransform(ID3D12GraphicsCommandList* commandList) override;

private:
	std::unique_ptr<ConstantBuffer> transformationMatrixConstantBuffer_;
	TransformationMatrix* transformationMatrix_ = nullptr;

	TransformComponent* transform_ = nullptr;
	StaticMesh* staticMesh_ = nullptr;
};
