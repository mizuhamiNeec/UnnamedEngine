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

	// TODO: 消す予定
	std::unique_ptr<ConstantBuffer> matparamCBV;
	MatParam* materialData = nullptr;

	// b1
	std::unique_ptr<ConstantBuffer> directionalLightCB;
	DirectionalLight* directionalLightData = nullptr;

	// b2
	std::unique_ptr<ConstantBuffer> cameraCB;
	CameraForGPU* cameraData = nullptr;

	// b3
	std::unique_ptr<ConstantBuffer> pointLightCB;
	PointLight* pointLightData = nullptr;

	// b4
	std::unique_ptr<ConstantBuffer> spotLightCB;
	SpotLight* spotLightData = nullptr;
};
