#pragma once
#include <engine/public/Components/MeshRenderer/Base/MeshRenderer.h>
#include <engine/public/Entity/Entity.h>
#include <engine/public/Renderer/ConstantBuffer.h>
#include <engine/public/ResourceSystem/Mesh/StaticMesh.h>

struct TransformationMatrix;
struct MatParam;
struct DirectionalLight;
struct CameraForGPU;
struct PointLight;
struct SpotLight;

class StaticMeshRenderer : public MeshRenderer {
public:
	StaticMeshRenderer() = default;
	~StaticMeshRenderer() override;

	void OnAttach(Entity& owner) override;

	void Render(ID3D12GraphicsCommandList* commandList) override;
	void DrawInspectorImGui() override;

	[[nodiscard]] StaticMesh* GetStaticMesh() const;
	void                      SetStaticMesh(StaticMesh* staticMesh);

protected:
	void BindTransform(ID3D12GraphicsCommandList* commandList) override;

private:
	std::unique_ptr<ConstantBuffer> mTransformationMatrixConstantBuffer;
	TransformationMatrix*           mTransformationMatrix = nullptr;

	StaticMesh*                     mStaticMesh = nullptr;
	std::unique_ptr<ConstantBuffer> mMatParamCBV;
	MatParam*                       mMaterialData = nullptr;

	// b1
	std::unique_ptr<ConstantBuffer> mDirectionalLightCb;
	DirectionalLight*               mDirectionalLightData = nullptr;

	// b2
	std::unique_ptr<ConstantBuffer> mCameraCb;
	CameraForGPU*                   mCameraData = nullptr;

	// b3
	std::unique_ptr<ConstantBuffer> mPointLightCb;
	PointLight*                     mPointLightData = nullptr;

	// b4
	std::unique_ptr<ConstantBuffer> mSpotLightCb;
	SpotLight*                      mSpotLightData = nullptr;
};
