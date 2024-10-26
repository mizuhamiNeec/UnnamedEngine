#include "Object3D.h"

#include "Object3DCommon.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Renderer/VertexBuffer.h"
#include "../Renderer/ConstantBuffer.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Math/MathLib.h"
#include "../Model/Model.h"


//-----------------------------------------------------------------------------
// Purpose : 初期化します
//-----------------------------------------------------------------------------
void Object3D::Init(Model* model, Object3DCommon* object3DCommon, ModelCommon* modelCommon) {

	// 引数で受け取ってメンバ変数に記録する
	this->model_ = model;
	this->object3DCommon_ = object3DCommon;
	this->modelCommon_ = modelCommon;

	// 座標変換行列定数バッファ
	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix));
	transformationMatrixData_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::Identity();
	transformationMatrixData_->world = Mat4::Identity();

	// 指向性ライト定数バッファ
	directionalLightConstantBuffer_ = std::make_unique<ConstantBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(DirectionalLight));
	directionalLightData_ = directionalLightConstantBuffer_->GetPtr<DirectionalLight>();
	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f }; // 白
	directionalLightData_->direction = { 0.0f,0.7071067812f,0.7071067812f }; // 斜め前向き
	directionalLightData_->intensity = 1.0f; // 明るさ1

	// 各トランスフォームの初期化
	transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	cameraTransform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
}

void Object3D::Update() {
#ifdef _DEBUG
	ImGui::Begin("Object3D");
	ImGui::DragFloat3("transform##obj", &transform_.translate.x, 0.01f);
	ImGui::DragFloat3("rotate##obj", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat3("scale##obj", &transform_.scale.x, 0.01f);
	ImGui::Separator();
	ImGui::DragFloat3("transform##cam", &cameraTransform_.translate.x, 0.01f);
	ImGui::DragFloat3("rotate##cam", &cameraTransform_.rotate.x, 0.01f);
	ImGui::DragFloat3("scale##cam", &cameraTransform_.scale.x, 0.01f);
	ImGui::End();
#endif


	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 cameraMat = Mat4::Affine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Mat4 viewMat = cameraMat.Inverse();
	Mat4 projMat = Mat4::PerspectiveFovMat(
		90.0f * Math::deg2Rad,
		static_cast<float>(object3DCommon_->GetD3D12()->GetWindow()->GetClientWidth()) / static_cast<float>(object3DCommon_->GetD3D12()->GetWindow()->GetClientHeight()),
		0.01f,
		1000.0f
	);
	transformationMatrixData_->wvp = worldMat * viewMat * projMat;
	transformationMatrixData_->world = worldMat;
}

void Object3D::Draw() const {
	// 座標変換行列の定数バッファの設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixConstantBuffer_->GetAddress());

	// 指向性ライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightConstantBuffer_->GetAddress());

	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}
}

void Object3D::SetModel(Model* model) {
	this->model_ = model;
}
