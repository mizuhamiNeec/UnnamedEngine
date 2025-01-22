#include "StaticMeshRenderer.h"

#include <d3d12.h>
#include <imgui.h>

#include <ResourceSystem/Mesh/StaticMesh.h>

#include "Engine.h"

#include "Camera/CameraManager.h"

#include "Components/Camera/CameraComponent.h"

#include "Renderer/ConstantBuffer.h"

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
		sizeof(TransformationMatrix)
	);
	transformationMatrix_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrix_->wvp = Mat4::identity;
	transformationMatrix_->world = Mat4::identity;
	transformationMatrix_->worldInverseTranspose = Mat4::identity;
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
			material->Apply(commandList); // Materialにバインド処理を委任
			// トランスフォームをバインド
			BindTransform(commandList);
			currentlyBoundMaterial = material;
		} else if (!material) {
			Console::Print("サブメッシュにマテリアルが設定されていません", kConsoleColorError, Channel::RenderSystem);
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
			ImGui::Text("Name: %s", staticMesh_->GetName().c_str());
			for (auto& subMesh : staticMesh_->GetSubMeshes()) {
				ImGui::Selectable((subMesh->GetMaterial()->GetName() + "##" + subMesh->GetName()).c_str());
			}
		}
	}
}

void StaticMeshRenderer::SetStaticMesh(StaticMesh* staticMesh) {
	staticMesh_ = staticMesh;
}

void StaticMeshRenderer::BindTransform(ID3D12GraphicsCommandList* commandList) {
	if (const auto* transform = transform_) {
		const Mat4 worldMat = transform->GetWorldMat();

		const Mat4& viewProjMat = CameraManager::GetActiveCamera()->GetViewProjMat();
		Mat4 worldViewProjMat = worldMat * viewProjMat;

		transformationMatrix_->wvp = worldViewProjMat;
		transformationMatrix_->world = worldMat;
		transformationMatrix_->worldInverseTranspose = worldMat.Inverse().Transpose();

		// ルートパラメータインデックスが正しいことを確認
		commandList->SetGraphicsRootConstantBufferView(
			0, // インデックスが正しいか確認
			transformationMatrixConstantBuffer_->GetAddress()
		);
	}
}
