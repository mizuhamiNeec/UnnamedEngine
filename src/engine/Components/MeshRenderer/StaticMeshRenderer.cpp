#include <engine/Components/MeshRenderer/StaticMeshRenderer.h>

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/ResourceSystem/Shader/Shader.h>
#include <engine/TextureManager/TexManager.h>

struct MatParam {
	Vec4  baseColor;
	float metallic;
	float roughness;
	float padding[2];
	Vec3  emissive;
};

StaticMeshRenderer::~StaticMeshRenderer() {
	mTransformationMatrixConstantBuffer.reset();
	mTransformationMatrix = nullptr;
	mScene                = nullptr;
	mStaticMesh           = nullptr;
}

void StaticMeshRenderer::OnAttach(Entity& owner) {
	MeshRenderer::OnAttach(owner);
	mScene = mOwner->GetTransform();

	mTransformationMatrixConstantBuffer = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(TransformationMatrix),
		"StaticMeshTransformation"
	);
	mTransformationMatrix = mTransformationMatrixConstantBuffer->GetPtr<
		TransformationMatrix>();
	mTransformationMatrix->wvp                   = Mat4::identity;
	mTransformationMatrix->world                 = Mat4::identity;
	mTransformationMatrix->worldInverseTranspose = Mat4::identity;

	{
		mMatParamCBV = std::make_unique<ConstantBuffer>(
			Unnamed::Engine::GetRenderer()->GetDevice(),
			sizeof(MatParam),
			"MatParam"
		);

		mMaterialData            = mMatParamCBV->GetPtr<MatParam>();
		mMaterialData->baseColor = {0.09f, 0.09f, 0.09f, 1.0f};
		mMaterialData->metallic  = 0.2f;
		mMaterialData->roughness = 1.0f;
		mMaterialData->emissive  = {0.0f, 0.0f, 0.0f};
	}

	mDirectionalLightCb = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(DirectionalLight),
		"DirectionalLight"
	);
	mDirectionalLightData = mDirectionalLightCb->GetPtr<DirectionalLight>();
	mDirectionalLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	mDirectionalLightData->direction = {-0.2f, -0.9f, 0.25f};
	mDirectionalLightData->intensity = 8.0f;

	mCameraCb = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(CameraForGPU),
		"Camera"
	);
	mCameraData                = mCameraCb->GetPtr<CameraForGPU>();
	mCameraData->worldPosition = Vec3::zero;

	mPointLightCb = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(PointLight),
		"PointLight"
	);
	mPointLightData            = mPointLightCb->GetPtr<PointLight>();
	mPointLightData->color     = {1.0f, 1.0f, 1.0f, 1.0f};
	mPointLightData->position  = {0.0f, 4.0f, 0.0f};
	mPointLightData->intensity = 1.0f;
	mPointLightData->radius    = 1.0f;
	mPointLightData->decay     = 1.0f;

	mSpotLightCb = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(SpotLight),
		"SpotLight"
	);
	mSpotLightData                  = mSpotLightCb->GetPtr<SpotLight>();
	mSpotLightData->color           = {1.0f, 1.0f, 1.0f, 1.0f};
	mSpotLightData->position        = {0.0f, 4.0f, 0.0f};
	mSpotLightData->intensity       = 1.0f;
	mSpotLightData->direction       = {0.0f, -1.0f, 0.0f};
	mSpotLightData->distance        = 8.0f;
	mSpotLightData->decay           = 2.0f;
	mSpotLightData->cosAngle        = 0.5f;
	mSpotLightData->cosFalloffStart = 0.5f;
}

void StaticMeshRenderer::Render(ID3D12GraphicsCommandList* commandList) {
	// メッシュが存在しない場合は描画をスキップ
	if (!mStaticMesh) {
		Console::Print("StaticMeshRenderer::Render - メッシュがnullです\n",
		               kConTextColorError, Channel::RenderSystem);
		return;
	}

	// 現在バインドされているマテリアルを追跡
	Material* currentlyBoundMaterial = nullptr;

	// メッシュ内の各サブメッシュを描画
	for (const auto& subMesh : mStaticMesh->GetSubMeshes()) {
		// 必要であればマテリアルをバインド
		Material* material = subMesh->GetMaterial();
		if (material && material != currentlyBoundMaterial) {
			// VS用のトランスフォーム (b0)
			if (const auto* transform = mScene) {
				const Mat4 worldMat = Mat4::Affine(
					transform->GetWorldScale(),
					transform->GetWorldRot().ToEulerAngles(),
					transform->GetWorldPos()
				);
				const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
					GetViewProjMat();
				Mat4 worldViewProjMat = worldMat * viewProjMat;

				mTransformationMatrix->wvp                   = worldViewProjMat;
				mTransformationMatrix->world                 = worldMat;
				mTransformationMatrix->worldInverseTranspose = worldMat.
					Inverse().Transpose();

				// VSのb0レジスタにバインド
				const UINT vsTransformRegister = material->GetShader()->
					GetResourceRegister("gTransformationMatrix");
				material->SetConstantBuffer(vsTransformRegister,
				                            mTransformationMatrixConstantBuffer
				                            ->GetResource());
			}

			// PS用の各種パラメータ
			const UINT materialRegister = material->GetShader()->
			                                        GetResourceRegister(
				                                        "gMaterial");
			material->SetConstantBuffer(materialRegister,
			                            mMatParamCBV->GetResource());

			const UINT dirLightRegister = material->GetShader()->
			                                        GetResourceRegister(
				                                        "gDirectionalLight");
			if (dirLightRegister < 0xffffffff) {
				material->SetConstantBuffer(dirLightRegister,
				                            mDirectionalLightCb->GetResource());
			}

			const UINT cameraRegister = material->GetShader()->
			                                      GetResourceRegister(
				                                      "gCamera");
			if (cameraRegister < 0xffffffff) {
				mCameraData->worldPosition = CameraManager::GetActiveCamera()->
				                             GetViewMat().Inverse().
				                             GetTranslate();
				material->SetConstantBuffer(cameraRegister,
				                            mCameraCb->GetResource());
			}

			// マテリアルのApply（すべてのテクスチャがディスクリプタテーブルでバインドされる）
			std::string meshName = mStaticMesh
				                       ? mStaticMesh->GetName()
				                       : "UnknownMesh";

			// ファイルパスからファイル名のみを抽出
			size_t lastSlash = meshName.find_last_of("/\\");
			if (lastSlash != std::string::npos) {
				meshName = meshName.substr(lastSlash + 1);
			}

			// 拡張子を削除
			size_t lastDot = meshName.find_last_of('.');
			if (lastDot != std::string::npos) {
				meshName = meshName.substr(0, lastDot);
			}

			material->Apply(commandList, meshName);

			// デバッグ用：テクスチャスロット確認のみ
			auto shader = material->GetShader();
			if (shader) {
				std::vector<std::string> textureOrder = shader->
					GetTextureSlots();
			}

			currentlyBoundMaterial = material;
		} else if (!material) {
			Console::Print(
				"サブメッシュにマテリアルが設定されていません\n",
				kConTextColorError,
				Channel::RenderSystem
			);
			continue;
		}

		// サブメッシュの描画
		subMesh->Render(commandList);
	}
}

void StaticMeshRenderer::DrawInspectorImGui() {
#ifdef _DEBUG
	// 子クラスのインスペクターUIの描画
	if (ImGui::CollapsingHeader("StaticMeshRenderer",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (mStaticMesh) {
			ImGui::Text("MatParams");
			ImGui::ColorEdit4("BaseColor", &mMaterialData->baseColor.x);
			ImGui::DragFloat("Metallic", &mMaterialData->metallic, 0.01f);
			ImGui::DragFloat("Roughness", &mMaterialData->roughness, 0.01f);
			ImGui::ColorEdit3("Emissive", &mMaterialData->emissive.x);

			ImGui::Separator();

			ImGui::Text("DirectionalLight");
			ImGui::ColorEdit4("Color##Directional",
			                  &mDirectionalLightData->color.x);
			if (ImGui::DragFloat3("Direction##Directional",
			                      &mDirectionalLightData->direction.x, 0.01f)) {
				mDirectionalLightData->direction.Normalize();
			}
			ImGui::DragFloat("Intensity##Directional",
			                 &mDirectionalLightData->intensity, 0.01f);

			ImGui::Text("CameraForGPU");
			ImGui::Text("World Position: %f, %f, %f",
			            mCameraData->worldPosition.x,
			            mCameraData->worldPosition.y,
			            mCameraData->worldPosition.z);

			ImGui::Text("PointLight");
			ImGui::ColorEdit4("Color##Point", &mPointLightData->color.x);
			ImGui::DragFloat3("Position##Point", &mPointLightData->position.x,
			                  0.01f);
			ImGui::DragFloat("Intensity##Point", &mPointLightData->intensity,
			                 0.01f);
			ImGui::DragFloat("Radius##Point", &mPointLightData->radius, 0.01f);
			ImGui::DragFloat("Decay##Point", &mPointLightData->decay, 0.01f);

			ImGui::Text("SpotLight");
			ImGui::ColorEdit4("Color##Spot", &mSpotLightData->color.x);
			ImGui::DragFloat3("Position##Spot", &mSpotLightData->position.x,
			                  0.01f);
			ImGui::DragFloat("Intensity##Spot", &mSpotLightData->intensity,
			                 0.01f);
			ImGui::DragFloat3("Direction##Spot", &mSpotLightData->direction.x,
			                  0.01f);
			ImGui::DragFloat("Distance##Spot", &mSpotLightData->distance,
			                 0.01f);
			ImGui::DragFloat("Decay##Spot", &mSpotLightData->decay, 0.01f);
			ImGui::DragFloat("CosAngle##Spot", &mSpotLightData->cosAngle,
			                 0.01f);
			ImGui::DragFloat("CosFalloff##Spot",
			                 &mSpotLightData->cosFalloffStart, 0.01f);

			ImGui::Text("Name: %s", mStaticMesh->GetName().c_str());

			// サブメッシュとテクスチャ情報の表示（デバッグ強化版）
			ImGui::Separator();
			ImGui::Text("SubMeshes and Textures:");
			for (auto& subMesh : mStaticMesh->GetSubMeshes()) {
				Material* material = subMesh->GetMaterial();
				if (material) {
					if (ImGui::TreeNode(
						(subMesh->GetName() + " - " + material->GetFullName()).
						c_str())) {
						// マテリアルのテクスチャ情報を表示
						const auto& textures = material->GetTextures();
						if (!textures.empty()) {
							ImGui::Text("Textures:");
							auto texManager = TexManager::GetInstance();

							for (const auto& [name, filePath] : textures) {
								// テクスチャ情報をより詳細に表示
								if (ImGui::TreeNode(
									(name + ": " + filePath).c_str())) {
									ImGui::Text("Slot名: %s", name.c_str());
									ImGui::Text("ファイルパス: %s", filePath.c_str());

									// テクスチャインデックス情報を表示
									uint32_t textureIndex = texManager->
										GetTextureIndexByFilePath(filePath);
									ImGui::Text("テクスチャインデックス: %u",
									            textureIndex);

									// テクスチャのプレビューを表示
									D3D12_GPU_DESCRIPTOR_HANDLE handle =
										texManager->GetSrvHandleGPU(filePath);
									if (handle.ptr != 0) {
										ImGui::Text(
											"GPU Handle: %llu", handle.ptr);
										ImGui::Image(
											(ImTextureID)handle.ptr,
											ImVec2(150, 150)
										);

										// メタデータ情報があれば表示
										try {
											const auto& metadata = texManager->
												GetMetaData(filePath);
											ImGui::Text("サイズ: %ux%u",
												static_cast<uint32_t>(metadata.
													width),
												static_cast<uint32_t>(metadata.
													height));
											ImGui::Text(
												"フォーマット: %d", metadata.format);
										} catch (...) {
											ImGui::Text("メタデータ取得エラー");
										}
									} else {
										ImGui::Text("テクスチャのGPUハンドルが無効です");
										// テクスチャロード試行
										if (ImGui::Button("テクスチャ再ロード")) {
											texManager->LoadTexture(filePath);
										}
									}
									ImGui::TreePop();
								}
							}
						} else {
							ImGui::Text("テクスチャなし");
						}
						ImGui::TreePop();
					}
				}
			}
		}
	}
#endif
}

StaticMesh* StaticMeshRenderer::GetStaticMesh() const {
	return mStaticMesh;
}

void StaticMeshRenderer::SetStaticMesh(StaticMesh* staticMesh) {
	mStaticMesh = staticMesh;
}

void StaticMeshRenderer::BindTransform(ID3D12GraphicsCommandList* commandList) {
	commandList;
}
