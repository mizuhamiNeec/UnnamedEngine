#include "Object3D.h"

#include "Object3DCommon.h"
#include "../Camera/Camera.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Model/Model.h"
#include "../Model/ModelManager.h"
#include "../Renderer/ConstantBuffer.h"

//-----------------------------------------------------------------------------
// Purpose : 初期化します
//-----------------------------------------------------------------------------
void Object3D::Init(Object3DCommon* object3DCommon, ModelCommon* modelCommon) {
	// 引数で受け取ってメンバ変数に記録する
	this->object3DCommon_ = object3DCommon;
	this->modelCommon_ = modelCommon;
	this->camera_ = object3DCommon_->GetDefaultCamera();

	// 座標変換行列定数バッファ
	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix));
	transformationMatrixData_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::Identity();
	transformationMatrixData_->world = Mat4::Identity();

	// 指向性ライト定数バッファ
	directionalLightConstantBuffer_ = std::make_unique<ConstantBuffer>(
		object3DCommon_->GetD3D12()->GetDevice(),
		sizeof(DirectionalLight)
	);
	directionalLightData_ = directionalLightConstantBuffer_->GetPtr<DirectionalLight>();
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
	directionalLightData_->direction = { 0.0f, -0.7071067812f, 0.7071067812f }; // 斜め前向き
	directionalLightData_->intensity = 1.0f; // 明るさ1
}

void Object3D::Update() {
#ifdef _DEBUG
	ImGui::Begin("Object3D");
	EditTransform("Object3D", transform_, 0.01f);
	if (ImGui::DragFloat3("direction##light", &directionalLightData_->direction.x, 0.01f)) {
		directionalLightData_->direction.Normalize();
	}
	ImGui::ColorEdit4("color##light", &directionalLightData_->color.x);
	ImGui::DragFloat("intensity##light", &directionalLightData_->intensity, 0.01f);
	ImGui::End();
#endif

	worldMat_ = Mat4::Affine(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	Mat4 worldViewProjMat;

	if (camera_) {
		// カメラが存在する場合はカメラから行列を持ってくる
		const Mat4& viewProjMat = camera_->GetViewProjMat();
		worldViewProjMat = worldMat_ * viewProjMat;
	} else {
		worldViewProjMat = worldMat_;
	}

	transformationMatrixData_->wvp = worldViewProjMat;
	transformationMatrixData_->world = worldMat_;
}

void Object3D::Draw() const {
	// 座標変換行列の定数バッファの設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		1, transformationMatrixConstantBuffer_->GetAddress());

	// 指向性ライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		3, directionalLightConstantBuffer_->GetAddress());

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

void Object3D::SetLighting(const bool& newLighting) const {
	model_->SetLighting(newLighting);
}