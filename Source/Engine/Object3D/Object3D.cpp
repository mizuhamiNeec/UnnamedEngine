#include "Object3D.h"

#include <Camera/CameraManager.h>

#include <ImGuiManager/ImGuiManager.h>

#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Math/Vector/Vec4.h>
#include <Lib/Structs/Structs.h>

#include <Model/Model.h>
#include <Model/ModelManager.h>

#include <Object3D/Object3DCommon.h>

#include <Renderer/ConstantBuffer.h>
#include <Renderer/D3D12.h>

//-----------------------------------------------------------------------------
// Purpose : 初期化します
//-----------------------------------------------------------------------------
void Object3D::Init(Object3DCommon* object3DCommon) {
	// 引数で受け取ってメンバ変数に記録する
	this->mObject3DCommon = object3DCommon;
	this->mCamera         = mObject3DCommon->GetDefaultCamera();

	// 座標変換行列定数バッファ
	mTransformationMatrixConstantBuffer = std::make_unique<ConstantBuffer>(
		mObject3DCommon->GetD3D12()->GetDevice(), sizeof(TransformationMatrix),
		"Object3DTransformation"
	);
	mTransformationMatrixData = mTransformationMatrixConstantBuffer->GetPtr<
		TransformationMatrix>();
	mTransformationMatrixData->wvp                   = Mat4::identity;
	mTransformationMatrixData->world                 = Mat4::identity;
	mTransformationMatrixData->worldInverseTranspose = Mat4::identity;

	// 指向性ライト定数バッファ
	mDirectionalLightConstantBuffer = std::make_unique<ConstantBuffer>(
		mObject3DCommon->GetD3D12()->GetDevice(),
		sizeof(DirectionalLight),
		"Object3DDirectionalLight"
	);
	mDirectionalLightData = mDirectionalLightConstantBuffer->GetPtr<
		DirectionalLight>();
	mDirectionalLightData->color     = {1.0f, 1.0f, 1.0f, 1.0f}; // 白
	mDirectionalLightData->direction = {-0.2f, -0.9f, 0.25f};
	mDirectionalLightData->intensity = 1.0f; // 明るさ1

	// カメラ定数バッファ
	mCameraConstantBuffer = std::make_unique<ConstantBuffer>(
		mObject3DCommon->GetD3D12()->GetDevice(), sizeof(CameraForGPU),
		"Object3DCamera"
	);
	mCameraForGPU = mCameraConstantBuffer->GetPtr<CameraForGPU>();
	mCameraForGPU->worldPosition = mCamera->GetViewMat().GetTranslate();

	// ポイントライト定数バッファ
	mPointLightConstantBuffer = std::make_unique<ConstantBuffer>(
		mObject3DCommon->GetD3D12()->GetDevice(),
		sizeof(PointLight),
		"Object3DPointLight"
	);
	mPointLightData = mPointLightConstantBuffer->GetPtr<PointLight>();
	mPointLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	mPointLightData->position = {0.0f, 4.0f, 0.0f};
	mPointLightData->intensity = 1.0f;
	mPointLightData->radius = 1.0f;
	mPointLightData->decay = 1.0f;

	// スポットライト定数バッファ
	mSpotLightConstantBuffer = std::make_unique<ConstantBuffer>(
		mObject3DCommon->GetD3D12()->GetDevice(),
		sizeof(SpotLight),
		"Object3DSpotLight"
	);
	mSpotLightData            = mSpotLightConstantBuffer->GetPtr<SpotLight>();
	mSpotLightData->color     = {1.0f, 1.0f, 1.0f, 1.0f};
	mSpotLightData->position  = {0.0f, 4.0f, 0.0f};
	mSpotLightData->intensity = 4.0f;
	mSpotLightData->direction = {0.0f, -1.0f, 0.0f};
	mSpotLightData->distance  = 8.0f;
	mSpotLightData->decay     = 2.0f;
	mSpotLightData->cosAngle  = 0.5f;
}

void Object3D::Update() {
#ifdef _DEBUG
	ImGui::Begin("Object3D");
	ImGuiManager::EditTransform(mTransform, 0.01f);
	if (ImGui::CollapsingHeader("Directional Light")) {
		if (ImGui::DragFloat3("dir##light", &mDirectionalLightData->direction.x,
		                      0.01f)) {
			mDirectionalLightData->direction.Normalize();
		}
		ImGui::ColorEdit4("color##light", &mDirectionalLightData->color.x);
		ImGui::DragFloat("intensity##light", &mDirectionalLightData->intensity,
		                 0.01f);
	}

	if (ImGui::CollapsingHeader("Point")) {
		ImGui::DragFloat3("pos##point", &mPointLightData->position.x, 0.01f);
		ImGui::ColorEdit4("color##point", &mPointLightData->color.x);
		ImGui::DragFloat("intensity##point", &mPointLightData->intensity,
		                 0.01f);
		ImGui::DragFloat("radius##point", &mPointLightData->radius, 0.01f);
		ImGui::DragFloat("decay##point", &mPointLightData->decay, 0.01f);
	}

	if (ImGui::CollapsingHeader("Spot")) {
		ImGui::ColorEdit4("color##spot", &mSpotLightData->color.x);
		ImGui::DragFloat3("pos##spot", &mSpotLightData->position.x, 0.01f);
		ImGui::DragFloat("intensity##spot", &mSpotLightData->intensity, 0.01f);
		if (ImGui::DragFloat3("direction##spot", &mSpotLightData->direction.x,
		                      0.01f)) {
			mSpotLightData->direction.Normalize();
		}
		ImGui::DragFloat("distance##spot", &mSpotLightData->distance, 0.01f);
		ImGui::DragFloat("decay##spot", &mSpotLightData->decay, 0.01f);
		ImGui::DragFloat("cosAngle##spot", &mSpotLightData->cosAngle, 0.01f);
		ImGui::DragFloat("cosFalloff##spot", &mSpotLightData->cosFalloffStart,
		                 0.01f);
	}
	ImGui::End();

	mModel->ImGuiDraw();
#endif
}

void Object3D::Draw() const {
	Mat4 worldMat = Mat4::Affine(
		mTransform.scale,
		mTransform.rotate,
		mTransform.translate
	);

	const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
		GetViewProjMat();
	Mat4 worldViewProjMat = worldMat * viewProjMat;

	mTransformationMatrixData->wvp                   = worldViewProjMat;
	mTransformationMatrixData->world                 = worldMat;
	mTransformationMatrixData->worldInverseTranspose = worldMat.Inverse().
		Transpose();

	mCameraForGPU->worldPosition = CameraManager::GetActiveCamera()->
	                               GetViewMat().Inverse().GetTranslate();

	// 座標変換行列の定数バッファの設定
	mObject3DCommon->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 1, mTransformationMatrixConstantBuffer->GetAddress()
	                 );

	// 指向性ライトの定数バッファを設定
	mObject3DCommon->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 3, mDirectionalLightConstantBuffer->GetAddress()
	                 );

	// カメラの定数バッファを設定
	mObject3DCommon->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 4, mCameraConstantBuffer->GetAddress()
	                 );

	// ポイントライトの定数バッファを設定
	mObject3DCommon->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 5, mPointLightConstantBuffer->GetAddress()
	                 );

	// スポットライトの定数バッファを設定
	mObject3DCommon->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 6, mSpotLightConstantBuffer->GetAddress()
	                 );

	// 3Dモデルが割り当てられていれば描画する
	if (mModel) {
		mModel->Draw();
	}
}

void Object3D::SetModel(Model* model) {
	this->mModel = model;
}

void Object3D::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	mModel = ModelManager::GetInstance()->FindModel(filePath);
}

void Object3D::SetScale(const Vec3& scale) {
	mTransform.scale = scale;
}

void Object3D::SetRot(const Vec3& newRot) {
	mTransform.rotate = newRot;
}

void Object3D::SetPos(const Vec3& newPos) {
	mTransform.translate = newPos;
}

const Vec3& Object3D::GetScale() const {
	return mTransform.scale;
}

const Vec3& Object3D::GetRot() const {
	return mTransform.rotate;
}

const Vec3& Object3D::GetPos() const {
	return mTransform.translate;
}
