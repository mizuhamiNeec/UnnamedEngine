#pragma once
#include <Components/MeshRenderer/Base/MeshRenderer.h>
#include <Entity/Base/Entity.h>
#include <Renderer/ConstantBuffer.h>
#include <ResourceSystem/Mesh/StaticMesh.h>
#include <ResourceSystem/Material/MaterialInstance.h>

class StaticMeshRenderer : public MeshRenderer {
public:
	StaticMeshRenderer() = default;
	~StaticMeshRenderer() override;

	void OnAttach(Entity& owner) override;

	// 描画処理
	void Render(ID3D12GraphicsCommandList* commandList) override;
	// インスペクターの描画
	void DrawInspectorImGui() override;

	[[nodiscard]] StaticMesh* GetStaticMesh() const;
	void SetStaticMesh(StaticMesh* staticMesh);

protected:
	void BindTransform(ID3D12GraphicsCommandList* commandList) override;

private:
	std::unique_ptr<ConstantBuffer> mTransformationMatrixConstantBuffer;
	TransformationMatrix* mTransformationMatrix = nullptr;

	TransformComponent* mTransform = nullptr;
	StaticMesh* mStaticMesh = nullptr;
	std::unique_ptr<ConstantBuffer> mMatParamCBV;
	MatParam* mMaterialData = nullptr;

	// b1
	std::unique_ptr<ConstantBuffer> mDirectionalLightCb;
	DirectionalLight* mDirectionalLightData = nullptr;

	// b2
	std::unique_ptr<ConstantBuffer> mCameraCb;
	CameraForGPU* mCameraData = nullptr;

	// b3
	std::unique_ptr<ConstantBuffer> mPointLightCb;
	PointLight* mPointLightData = nullptr;

	// b4
	std::unique_ptr<ConstantBuffer> mSpotLightCb;
	SpotLight* mSpotLightData = nullptr;
};
