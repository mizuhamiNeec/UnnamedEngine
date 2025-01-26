#include "Object3D.h"

#include "../Camera/Camera.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Model/Model.h"
#include "../Model/ModelManager.h"
#include "../Renderer/ConstantBuffer.h"
#include "Object3DCommon.h"
#include "Camera/CameraManager.h"

//-----------------------------------------------------------------------------
// Purpose : 初期化します
//-----------------------------------------------------------------------------
void Object3D::Init(Object3DCommon* object3DCommon) {
	// 引数で受け取ってメンバ変数に記録する
	this->object3DCommon_ = object3DCommon;
	this->camera_ = object3DCommon_->GetDefaultCamera();

	// 座標変換行列定数バッファ
	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix)
	);
	transformationMatrixData_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::identity;
	transformationMatrixData_->world = Mat4::identity;

	// 指向性ライト定数バッファ
	directionalLightConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(),
		sizeof(DirectionalLight)
	);
	directionalLightData_ = directionalLightConstantBuffer_->GetPtr<DirectionalLight>();
	directionalLightData_->color = { 1.0f, 0.0f, 0.0f, 1.0f };
	directionalLightData_->direction = { -0.2f, -0.9f, 0.25f };
	directionalLightData_->intensity = 1.0f; // 明るさ1

	// カメラ定数バッファ
	cameraConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(), sizeof(CameraForGPU)
	);
	cameraForGPU_ = cameraConstantBuffer_->GetPtr<CameraForGPU>();
	cameraForGPU_->worldPosition = camera_->GetViewMat().GetTranslate();

	// ポイントライト定数バッファ
	pointLightConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(),
		sizeof(PointLight)
	);
	pointLightData_ = pointLightConstantBuffer_->GetPtr<PointLight>();
	pointLightData_->color = { 0.0f, 1.0f, 0.0f, 1.0f };
	pointLightData_->position = { -1.0f, 1.3f, -0.0f };
	pointLightData_->intensity = 10.0f;
	pointLightData_->radius = 1.0f;
	pointLightData_->decay = 1.0f;

	// スポットライト定数バッファ
	spotLightConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(),
		sizeof(SpotLight)
	);
	spotLightData_ = spotLightConstantBuffer_->GetPtr<SpotLight>();
	spotLightData_->color = { 0.0f, 0.0f, 1.0f, 1.0f };
	spotLightData_->position = { 3.0f, 3.0f, -6.0f };
	spotLightData_->intensity = 10.0f;
	spotLightData_->direction = { 0.0f, -1.0f, 0.0f };
	spotLightData_->distance = 10.0f;
	spotLightData_->decay = 2.0f;
	spotLightData_->cosAngle = 0.5f;
}

void Object3D::Update() {
#ifdef _DEBUG
	ImGui::Begin("Object3D");
	ImGuiManager::EditTransform(transform_, 0.01f);
	if (ImGui::CollapsingHeader("Directional Light")) {
		if (ImGui::DragFloat3("dir##light", &directionalLightData_->direction.x, 0.01f)) {
			directionalLightData_->direction.Normalize();
		}
		ImGui::ColorEdit4("color##light", &directionalLightData_->color.x);
		ImGui::DragFloat("intensity##light", &directionalLightData_->intensity, 0.01f);
	}

	if (ImGui::CollapsingHeader("Point")) {
		ImGui::DragFloat3("pos##point", &pointLightData_->position.x, 0.01f);
		ImGui::ColorEdit4("color##point", &pointLightData_->color.x);
		ImGui::DragFloat("intensity##point", &pointLightData_->intensity, 0.01f);
		ImGui::DragFloat("radius##point", &pointLightData_->radius, 0.01f);
		ImGui::DragFloat("decay##point", &pointLightData_->decay, 0.01f);
	}

	if (ImGui::CollapsingHeader("Spot")) {
		ImGui::ColorEdit4("color##spot", &spotLightData_->color.x);
		ImGui::DragFloat3("pos##spot", &spotLightData_->position.x, 0.01f);
		ImGui::DragFloat("intensity##spot", &spotLightData_->intensity, 0.01f);
		if (ImGui::DragFloat3("direction##spot", &spotLightData_->direction.x, 0.01f)) {
			spotLightData_->direction.Normalize();
		}
		ImGui::DragFloat("distance##spot", &spotLightData_->distance, 0.01f);
		ImGui::DragFloat("decay##spot", &spotLightData_->decay, 0.01f);
		ImGui::DragFloat("cosAngle##spot", &spotLightData_->cosAngle, 0.01f);
		ImGui::DragFloat("cosFalloff##spot", &spotLightData_->cosFalloffStart, 0.01f);
	}
	ImGui::End();

	model_->ImGuiDraw();
#endif
}

void Object3D::Draw() const {
	Mat4 worldMat = Mat4::Affine(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	const Mat4& viewProjMat = CameraManager::GetActiveCamera()->GetViewProjMat();
	Mat4 worldViewProjMat = worldMat * viewProjMat;

	transformationMatrixData_->wvp = worldViewProjMat;
	transformationMatrixData_->world = worldMat;
	transformationMatrixData_->worldInverseTranspose = worldMat.Inverse().Transpose();

	cameraForGPU_->worldPosition = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate();

	// 座標変換行列の定数バッファの設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		1, transformationMatrixConstantBuffer_->GetAddress()
	);

	// 指向性ライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		3, directionalLightConstantBuffer_->GetAddress()
	);

	// カメラの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		4, cameraConstantBuffer_->GetAddress()
	);

	// ポイントライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		5, pointLightConstantBuffer_->GetAddress()
	);

	// スポットライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		6, spotLightConstantBuffer_->GetAddress()
	);

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}
}

void Object3D::SetModel(Model* model) {
	this->model_ = model;
}

void Object3D::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}
