#include "StaticMeshRenderer.h"

#include <d3d12.h>
#include <imgui.h>

#include <ResourceSystem/Mesh/StaticMesh.h>

#include "Engine.h"

#include "Camera/CameraManager.h"

#include "Components/Camera/CameraComponent.h"

#include "Renderer/ConstantBuffer.h"

// TODO: 後で消す Object3Dシェーダーとはおさらばじゃ!!
struct MatParam {
	Vec4 color;
	int32_t enableLighting;
	Vec3 padding;
	Mat4 uvTransform;
	float shininess;
	Vec3 specularColor;
};

StaticMeshRenderer::~StaticMeshRenderer() {
	transformationMatrixConstantBuffer_.reset();
	transformationMatrix_ = nullptr;
	transform_ = nullptr;
	staticMesh_ = nullptr;
}

void StaticMeshRenderer::OnAttach(Entity& owner) {
	MeshRenderer::OnAttach(owner);
	transform_ = owner_->GetTransform();

	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(TransformationMatrix),
		"StaticMeshTransformation"
	);
	transformationMatrix_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrix_->wvp = Mat4::identity;
	transformationMatrix_->world = Mat4::identity;
	transformationMatrix_->worldInverseTranspose = Mat4::identity;

	// TODO: 消す予定
	{
		matparamCBV = std::make_unique<ConstantBuffer>(
			Engine::GetRenderer()->GetDevice(),
			sizeof(MatParam),
			"MatParam"
		);

		materialData = matparamCBV->GetPtr<MatParam>();
		materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData->enableLighting = 1;
		materialData->uvTransform = Mat4::identity;
		materialData->shininess = 128.0f;
		materialData->specularColor = { 0.25f, 0.25f, 0.25f };
	}

	cameraCB = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(CameraForGPU),
		"Camera"
	);

	cameraData = cameraCB->GetPtr<CameraForGPU>();
	cameraData->worldPosition = CameraManager::GetActiveCamera()->GetViewMat().GetTranslate();
}

void StaticMeshRenderer::Render(ID3D12GraphicsCommandList* commandList) {
	// メッシュが存在しない場合は描画をスキップ
	if (!staticMesh_) return;

	// 現在バインドされているマテリアルを追跡
	Material* currentlyBoundMaterial = nullptr;

	// メッシュ内の各サブメッシュを描画
	for (const auto& subMesh : staticMesh_->GetSubMeshes()) {
		// 必要であればマテリアルをバインド
		Material* material = subMesh->GetMaterial();
		if (material && material != currentlyBoundMaterial) {

			material->SetConstantBuffer(0, matparamCBV->GetResource());

			if (const auto* transform = transform_) {
				const Mat4 worldMat = transform->GetWorldMat();

				const Mat4& viewProjMat = CameraManager::GetActiveCamera()->GetViewProjMat();
				Mat4 worldViewProjMat = worldMat * viewProjMat;

				transformationMatrix_->wvp = worldViewProjMat;
				transformationMatrix_->world = worldMat;
				transformationMatrix_->worldInverseTranspose = worldMat.Inverse().Transpose();

				//// ルートパラメータインデックスが正しいことを確認
				//commandList->SetGraphicsRootConstantBufferView(
				//	1, // インデックスが正しいか確認
				//	transformationMatrixConstantBuffer_->GetAddress()
				//);

				material->SetConstantBuffer(1, transformationMatrixConstantBuffer_->GetResource());
			}

			cameraData->worldPosition = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate();
			material->SetConstantBuffer(2, cameraCB->GetResource());

			// トランスフォームをバインド
			BindTransform(commandList);

			material->Apply(commandList); // Materialにバインド処理を委任
			currentlyBoundMaterial = material;
		} else if (!material) {
			Console::Print(
				"サブメッシュにマテリアルが設定されていません",
				kConsoleColorError,
				Channel::RenderSystem
			);
			continue;
		}

		// サブメッシュの描画
		subMesh->Render(commandList);
	}
}

void StaticMeshRenderer::DrawInspectorImGui() {
	// 子クラスのインスペクターUIの描画
	if (ImGui::CollapsingHeader("StaticMeshRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (staticMesh_) {
			ImGui::Text("MatParams");
			ImGui::ColorEdit4("Color", &materialData->color.x);
			static bool enableLighting = true;
			if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
				materialData->enableLighting = enableLighting ? 1 : 0;
			}
			ImGui::SliderFloat("Shininess", &materialData->shininess, 0.0f, 128.0f);
			ImGui::ColorEdit3("Specular Color", &materialData->specularColor.x);

			ImGui::Text("CameraForGPU");
			ImGui::Text("World Position: %f, %f, %f", cameraData->worldPosition.x, cameraData->worldPosition.y, cameraData->worldPosition.z);

			ImGui::Text("Name: %s", staticMesh_->GetName().c_str());
			for (auto& subMesh : staticMesh_->GetSubMeshes()) {
				ImGui::Selectable((subMesh->GetMaterial()->GetName() + "##" + subMesh->GetName()).c_str());
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
