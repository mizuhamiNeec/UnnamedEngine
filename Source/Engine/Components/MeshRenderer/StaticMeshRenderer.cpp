#include "StaticMeshRenderer.h"

#include <d3d12.h>
#include <Engine.h>
#include <imgui.h>
#include <Camera/CameraManager.h>
#include <Components/Camera/CameraComponent.h>
#include <Renderer/ConstantBuffer.h>
#include <ResourceSystem/Mesh/StaticMesh.h>

// TODO: 後で消す Object3Dシェーダーとはおさらばじゃ!!
struct MatParam {
	Vec4  baseColor;
	float metallic;
	float roughness;
	float padding[2];
	Vec3  emissive;
};

StaticMeshRenderer::~StaticMeshRenderer() {
	transformationMatrixConstantBuffer_.reset();
	transformationMatrix_ = nullptr;
	transform_            = nullptr;
	staticMesh_           = nullptr;
}

void StaticMeshRenderer::OnAttach(Entity& owner) {
	MeshRenderer::OnAttach(owner);
	transform_ = owner_->GetTransform();

	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(TransformationMatrix),
		"StaticMeshTransformation"
	);
	transformationMatrix_ = transformationMatrixConstantBuffer_->GetPtr<
		TransformationMatrix>();
	transformationMatrix_->wvp                   = Mat4::identity;
	transformationMatrix_->world                 = Mat4::identity;
	transformationMatrix_->worldInverseTranspose = Mat4::identity;

	// TODO: 消す予定
	{
		matparamCBV = std::make_unique<ConstantBuffer>(
			Engine::GetRenderer()->GetDevice(),
			sizeof(MatParam),
			"MatParam"
		);

		materialData            = matparamCBV->GetPtr<MatParam>();
		materialData->baseColor = {0.5f, 0.5f, 0.5f, 1.0f};
		materialData->metallic  = 0.25f;
		materialData->roughness = 0.5f;
		materialData->emissive  = {0.0f, 0.0f, 0.0f};
	}

	directionalLightCB = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(DirectionalLight),
		"DirectionalLight"
	);
	directionalLightData = directionalLightCB->GetPtr<DirectionalLight>();
	directionalLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	directionalLightData->direction = {-0.2f, -0.9f, 0.25f};
	directionalLightData->intensity = 8.0f;

	cameraCB = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(CameraForGPU),
		"Camera"
	);
	cameraData                = cameraCB->GetPtr<CameraForGPU>();
	cameraData->worldPosition = CameraManager::GetActiveCamera()->GetViewMat().
		GetTranslate();

	pointLightCB = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(PointLight),
		"PointLight"
	);
	pointLightData            = pointLightCB->GetPtr<PointLight>();
	pointLightData->color     = {1.0f, 1.0f, 1.0f, 1.0f};
	pointLightData->position  = {0.0f, 4.0f, 0.0f};
	pointLightData->intensity = 1.0f;
	pointLightData->radius    = 1.0f;
	pointLightData->decay     = 1.0f;

	spotLightCB = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(SpotLight),
		"SpotLight"
	);
	spotLightData                  = spotLightCB->GetPtr<SpotLight>();
	spotLightData->color           = {1.0f, 1.0f, 1.0f, 1.0f};
	spotLightData->position        = {0.0f, 4.0f, 0.0f};
	spotLightData->intensity       = 1.0f;
	spotLightData->direction       = {0.0f, -1.0f, 0.0f};
	spotLightData->distance        = 8.0f;
	spotLightData->decay           = 2.0f;
	spotLightData->cosAngle        = 0.5f;
	spotLightData->cosFalloffStart = 0.5f;
}

void StaticMeshRenderer::Render(ID3D12GraphicsCommandList* commandList) {
	// メッシュが存在しない場合は描画をスキップ
	if (!staticMesh_)
		return;

	// 現在バインドされているマテリアルを追跡
	Material* currentlyBoundMaterial = nullptr;

	// メッシュ内の各サブメッシュを描画
	for (const auto& subMesh : staticMesh_->GetSubMeshes()) {
		// 必要であればマテリアルをバインド
		Material* material = subMesh->GetMaterial();
		if (material && material != currentlyBoundMaterial) {
			// VS用のトランスフォーム (b0)
			if (const auto* transform = transform_) {
				const Mat4 worldMat = Mat4::Affine(
					transform->GetWorldScale(),
					transform->GetWorldRot().ToEulerAngles(),
					transform->GetWorldPos()
				);
				const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
					GetViewProjMat();
				Mat4 worldViewProjMat = worldMat * viewProjMat;

				transformationMatrix_->wvp                   = worldViewProjMat;
				transformationMatrix_->world                 = worldMat;
				transformationMatrix_->worldInverseTranspose = worldMat.
					Inverse().Transpose();

				// VSのb0レジスタにバインド
				const UINT vsTransformRegister = material->GetShader()->
					GetResourceRegister("gTransformationMatrix");
				material->SetConstantBuffer(vsTransformRegister,
				                            transformationMatrixConstantBuffer_
				                            ->GetResource());
			}

			// PS用の各種パラメータ
			const UINT materialRegister = material->GetShader()->
			                                        GetResourceRegister(
				                                        "gMaterial");
			material->SetConstantBuffer(materialRegister,
			                            matparamCBV->GetResource());

			const UINT dirLightRegister = material->GetShader()->
			                                        GetResourceRegister(
				                                        "gDirectionalLight");
			if (dirLightRegister < 0xffffffff) {
				material->SetConstantBuffer(dirLightRegister,
				                            directionalLightCB->GetResource());
			}

			const UINT cameraRegister = material->GetShader()->
			                                      GetResourceRegister(
				                                      "gCamera");
			if (cameraRegister < 0xffffffff) {
				cameraData->worldPosition = CameraManager::GetActiveCamera()->
				                            GetViewMat().Inverse().
				                            GetTranslate();
				material->SetConstantBuffer(cameraRegister,
				                            cameraCB->GetResource());
			}

			material->Apply(commandList);
			currentlyBoundMaterial = material;
		} else if (!material) {
			Console::Print(
				"サブメッシュにマテリアルが設定されていません",
				kConTextColorError,
				Channel::RenderSystem
			);
			continue;
		}

		// サブメッシュの描画
		subMesh->Render(commandList);
	}
}

void MatrixEdit(const std::string& label, Mat4& mat) {
	ImGui::Text(label.c_str());
	ImGui::DragFloat4((label + "##" + std::to_string(0)).c_str(), &mat.m[0][0],
	                  0.01f);
	ImGui::DragFloat4((label + "##" + std::to_string(1)).c_str(), &mat.m[1][0],
	                  0.01f);
	ImGui::DragFloat4((label + "##" + std::to_string(2)).c_str(), &mat.m[2][0],
	                  0.01f);
	ImGui::DragFloat4((label + "##" + std::to_string(3)).c_str(), &mat.m[3][0],
	                  0.01f);
}

void StaticMeshRenderer::DrawInspectorImGui() {
	// 子クラスのインスペクターUIの描画
	if (ImGui::CollapsingHeader("StaticMeshRenderer",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (staticMesh_) {
			ImGui::Text("MatParams");
			ImGui::ColorEdit4("BaseColor", &materialData->baseColor.x);
			ImGui::DragFloat("Metallic", &materialData->metallic, 0.01f);
			ImGui::DragFloat("Roughness", &materialData->roughness, 0.01f);
			ImGui::ColorEdit3("Emissive", &materialData->emissive.x);

			ImGui::Separator();

			ImGui::Text("DirectionalLight");
			ImGui::ColorEdit4("Color##Directional",
			                  &directionalLightData->color.x);
			if (ImGui::DragFloat3("Direction##Directional",
			                      &directionalLightData->direction.x, 0.01f)) {
				directionalLightData->direction.Normalize();
			}
			ImGui::DragFloat("Intensity##Directional",
			                 &directionalLightData->intensity, 0.01f);

			ImGui::Text("CameraForGPU");
			ImGui::Text("World Position: %f, %f, %f",
			            cameraData->worldPosition.x,
			            cameraData->worldPosition.y,
			            cameraData->worldPosition.z);

			ImGui::Text("PointLight");
			ImGui::ColorEdit4("Color##Point", &pointLightData->color.x);
			ImGui::DragFloat3("Position##Point", &pointLightData->position.x,
			                  0.01f);
			ImGui::DragFloat("Intensity##Point", &pointLightData->intensity,
			                 0.01f);
			ImGui::DragFloat("Radius##Point", &pointLightData->radius, 0.01f);
			ImGui::DragFloat("Decay##Point", &pointLightData->decay, 0.01f);

			ImGui::Text("SpotLight");
			ImGui::ColorEdit4("Color##Spot", &spotLightData->color.x);
			ImGui::DragFloat3("Position##Spot", &spotLightData->position.x,
			                  0.01f);
			ImGui::DragFloat("Intensity##Spot", &spotLightData->intensity,
			                 0.01f);
			ImGui::DragFloat3("Direction##Spot", &spotLightData->direction.x,
			                  0.01f);
			ImGui::DragFloat("Distance##Spot", &spotLightData->distance, 0.01f);
			ImGui::DragFloat("Decay##Spot", &spotLightData->decay, 0.01f);
			ImGui::DragFloat("CosAngle##Spot", &spotLightData->cosAngle, 0.01f);
			ImGui::DragFloat("CosFalloff##Spot",
			                 &spotLightData->cosFalloffStart, 0.01f);

			ImGui::Text("Name: %s", staticMesh_->GetName().c_str());
			for (auto& subMesh : staticMesh_->GetSubMeshes()) {
				ImGui::Selectable(
					(subMesh->GetMaterial()->GetName() + "##" + subMesh->
						GetName()).c_str());
			}
		}
	}
}

StaticMesh* StaticMeshRenderer::GetStaticMesh() const {
	return staticMesh_;
}

void StaticMeshRenderer::SetStaticMesh(StaticMesh* staticMesh) {
	staticMesh_ = staticMesh;
}

void StaticMeshRenderer::BindTransform(ID3D12GraphicsCommandList* commandList) {
	commandList;
}
