#include "SkeletalMeshRenderer.h"

#include <d3d12.h>
#include <Engine.h>
#include <imgui.h>
#include <Camera/CameraManager.h>
#include <Components/Camera/CameraComponent.h>
#include <Renderer/ConstantBuffer.h>
#include <ResourceSystem/Mesh/SkeletalMesh.h>
#include <Animation/KeyFrame.h>

#include "Debug/Debug.h"

#include "Lib/DebugHud/DebugHud.h"

// TODO: 後で消す Object3Dシェーダーとはおさらばじゃ!!
struct MatParam {
	Vec4  baseColor;
	float metallic;
	float roughness;
	float padding[2];
	Vec3  emissive;
};

SkeletalMeshRenderer::~SkeletalMeshRenderer() {
	transformationMatrixConstantBuffer_.reset();
	boneMatricesConstantBuffer_.reset();
	transformationMatrix_ = nullptr;
	boneMatrices_         = nullptr;
	transform_            = nullptr;
	skeletalMesh_         = nullptr;
	currentAnimation_     = nullptr;
}

void SkeletalMeshRenderer::OnAttach(Entity& owner) {
	MeshRenderer::OnAttach(owner);
	transform_ = mOwner->GetTransform();

	// 変換行列用の定数バッファ
	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(TransformationMatrix),
		"SkeletalMeshTransformation"
	);
	transformationMatrix_ = transformationMatrixConstantBuffer_->GetPtr<
		TransformationMatrix>();
	transformationMatrix_->wvp                   = Mat4::identity;
	transformationMatrix_->world                 = Mat4::identity;
	transformationMatrix_->worldInverseTranspose = Mat4::identity;

	// ボーン変換行列用の定数バッファ
	boneMatricesConstantBuffer_ = std::make_unique<ConstantBuffer>(
		Engine::GetRenderer()->GetDevice(),
		sizeof(BoneMatrices),
		"BoneMatrices"
	);
	boneMatrices_ = boneMatricesConstantBuffer_->GetPtr<BoneMatrices>();

	// ボーン行列を単位行列で初期化
	for (int i = 0; i < BoneMatrices::MAX_BONES; ++i) {
		boneMatrices_->bones[i] = Mat4::identity;
	}

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
	cameraData->worldPosition = Vec3::zero;

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

void SkeletalMeshRenderer::Update(float deltaTime) {
	if (isPlaying_ && currentAnimation_) {
		animationTime_ += deltaTime * animationSpeed_;

		if (isLooping_) {
			// 最後まで行ったらリピート再生
			animationTime_ = std::fmod(animationTime_,
			                           currentAnimation_->duration);
		} else if (animationTime_ >= currentAnimation_->duration) {
			// ループしない場合は停止
			animationTime_ = currentAnimation_->duration;
			isPlaying_     = false;
		}

		// ボーン変換行列を更新
		UpdateBoneMatrices();
	}
	if (showBoneDebug_) {
		DrawBoneDebug();
	}
}

void SkeletalMeshRenderer::Render(ID3D12GraphicsCommandList* commandList) {
	// メッシュが存在しない場合は描画をスキップ
	if (!skeletalMesh_) {
		Console::Print("SkeletalMeshRenderer::Render - メッシュがnullです\n",
		               kConTextColorError, Channel::RenderSystem);
		return;
	}

	// 現在バインドされているマテリアルを追跡
	Material* currentlyBoundMaterial = nullptr;

	// メッシュ内の各サブメッシュを描画
	for (const auto& subMesh : skeletalMesh_->GetSubMeshes()) {
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

			// ボーン変換行列をバインド (通常はb5)
			const UINT boneMatricesRegister = material->GetShader()->
				GetResourceRegister("gBoneMatrices");
			if (boneMatricesRegister < 0xffffffff) {
				material->SetConstantBuffer(boneMatricesRegister,
				                            boneMatricesConstantBuffer_->
				                            GetResource());
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

			// マテリアルのApply（すべてのテクスチャがディスクリプタテーブルでバインドされる）
			std::string meshName = skeletalMesh_
				                       ? skeletalMesh_->GetName()
				                       : "UnknownSkeletalMesh";

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

void SkeletalMeshRenderer::DrawInspectorImGui() {
	// 子クラスのインスペクターUIの描画
	if (ImGui::CollapsingHeader("SkeletalMeshRenderer",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (skeletalMesh_) {
			ImGui::Checkbox("Show Bone Debug", &showBoneDebug_);

			if (ImGui::TreeNode("Animation")) {
				// アニメーション制御UI
				ImGui::Text("Animation Control");
				ImGui::Separator();

				// アニメーション選択
				const auto& animations = skeletalMesh_->GetAnimations();
				if (!animations.empty()) {
					if (ImGui::BeginCombo("Animation",
					                      currentAnimationName_.c_str())) {
						for (const auto& [animName, anim] : animations) {
							bool isSelected = (currentAnimationName_ ==
								animName);
							if (ImGui::Selectable(
								animName.c_str(), isSelected)) {
								PlayAnimation(animName, isLooping_);
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					// 再生制御
					if (ImGui::Button(isPlaying_ ? "Pause" : "Play")) {
						if (isPlaying_) {
							PauseAnimation();
						} else {
							ResumeAnimation();
						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Stop")) {
						StopAnimation();
					}

					// アニメーション設定
					ImGui::Checkbox("Loop", &isLooping_);
					ImGui::DragFloat("Speed", &animationSpeed_, 0.1f, 0.1f,
					                 5.0f);

					if (currentAnimation_) {
						ImGui::Text("Time: %.2f / %.2f", animationTime_,
						            currentAnimation_->duration);

						// アニメーション時間スライダー
						float normalizedTime = currentAnimation_->duration > 0
							                       ? animationTime_ /
							                       currentAnimation_->duration
							                       : 0.0f;
						if (ImGui::SliderFloat("Progress", &normalizedTime,
						                       0.0f,
						                       1.0f)) {
							animationTime_ = normalizedTime * currentAnimation_
								->
								duration;
							UpdateBoneMatrices();
						}
					}
				} else {
					ImGui::Text("No animations available");
				}
				ImGui::TreePop();
			}

			ImGui::Separator();

			if (ImGui::TreeNode("Material")) {
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
				                      &directionalLightData->direction.x,
				                      0.01f)) {
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
				ImGui::DragFloat3("Position##Point",
				                  &pointLightData->position.x,
				                  0.01f);
				ImGui::DragFloat("Intensity##Point", &pointLightData->intensity,
				                 0.01f);
				ImGui::DragFloat("Radius##Point", &pointLightData->radius,
				                 0.01f);
				ImGui::DragFloat("Decay##Point", &pointLightData->decay, 0.01f);

				ImGui::Text("SpotLight");
				ImGui::ColorEdit4("Color##Spot", &spotLightData->color.x);
				ImGui::DragFloat3("Position##Spot", &spotLightData->position.x,
				                  0.01f);
				ImGui::DragFloat("Intensity##Spot", &spotLightData->intensity,
				                 0.01f);
				ImGui::DragFloat3("Direction##Spot",
				                  &spotLightData->direction.x,
				                  0.01f);
				ImGui::DragFloat("Distance##Spot", &spotLightData->distance,
				                 0.01f);
				ImGui::DragFloat("Decay##Spot", &spotLightData->decay, 0.01f);
				ImGui::DragFloat("CosAngle##Spot", &spotLightData->cosAngle,
				                 0.01f);
				ImGui::DragFloat("CosFalloff##Spot",
				                 &spotLightData->cosFalloffStart, 0.01f);

				ImGui::TreePop();
			}

			ImGui::Separator();

			// スケルトン情報表示
			const Skeleton& skeleton = skeletalMesh_->GetSkeleton();
			if (ImGui::TreeNode("Skeleton Info")) {
				ImGui::Text("Bone Count: %zu", skeleton.bones.size());
				for (size_t i = 0; i < skeleton.bones.size() && i < 10; ++i) {
					ImGui::Text("Bone %zu: %s", i,
					            skeleton.bones[i].name.c_str());
				}
				if (skeleton.bones.size() > 10) {
					ImGui::Text("... and %zu more bones",
					            skeleton.bones.size() - 10);
				}
				ImGui::TreePop();
			}

			ImGui::Text("Name: %s", skeletalMesh_->GetName().c_str());

			// サブメッシュとテクスチャ情報の表示
			ImGui::Separator();
			ImGui::Text("SubMeshes and Textures:");
			for (auto& subMesh : skeletalMesh_->GetSubMeshes()) {
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
											ImVec2(150, 150));
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
}

SkeletalMesh* SkeletalMeshRenderer::GetSkeletalMesh() const {
	return skeletalMesh_;
}

void SkeletalMeshRenderer::SetSkeletalMesh(SkeletalMesh* skeletalMesh) {
	skeletalMesh_ = skeletalMesh;

	// 最初のアニメーションを自動選択
	if (skeletalMesh_ && !skeletalMesh_->GetAnimations().empty()) {
		const auto& animations = skeletalMesh_->GetAnimations();
		const auto& firstAnim  = animations.begin();
		currentAnimationName_  = firstAnim->first;
		currentAnimation_      = &firstAnim->second;
		animationTime_         = 0.0f;
	}
}

void SkeletalMeshRenderer::PlayAnimation(const std::string& animationName,
                                         bool               loop) {
	if (!skeletalMesh_) return;

	const Animation* animation = skeletalMesh_->GetAnimation(animationName);
	if (animation) {
		currentAnimation_     = animation;
		currentAnimationName_ = animationName;
		isLooping_            = loop;
		isPlaying_            = true;
		animationTime_        = 0.0f;

		Console::Print("アニメーション再生開始: " + animationName + "\n",
		               kConTextColorCompleted, Channel::RenderSystem);
	} else {
		Console::Print("アニメーションが見つかりません: " + animationName + "\n",
		               kConTextColorError, Channel::RenderSystem);
	}
}

void SkeletalMeshRenderer::StopAnimation() {
	isPlaying_     = false;
	animationTime_ = 0.0f;
}

void SkeletalMeshRenderer::PauseAnimation() {
	isPlaying_ = false;
}

void SkeletalMeshRenderer::ResumeAnimation() {
	if (currentAnimation_) {
		isPlaying_ = true;
	}
}

void SkeletalMeshRenderer::SetAnimationSpeed(float speed) {
	animationSpeed_ = speed;
}

bool SkeletalMeshRenderer::IsAnimationPlaying() const {
	return isPlaying_;
}

float SkeletalMeshRenderer::GetAnimationTime() const {
	return animationTime_;
}

void SkeletalMeshRenderer::BindTransform(
	ID3D12GraphicsCommandList* commandList) {
	commandList;
}

void SkeletalMeshRenderer::UpdateBoneMatrices() {
	if (!skeletalMesh_ || !currentAnimation_) return;

	const Skeleton& skeleton = skeletalMesh_->GetSkeleton();

	// ボーン行列を単位行列で初期化
	for (int i = 0; i < BoneMatrices::MAX_BONES; ++i) {
		boneMatrices_->bones[i] = Mat4::identity;
	}

	// ルートノードから開始して変換行列を計算
	CalculateNodeTransform(skeleton.rootNode, Mat4::identity, currentAnimation_,
	                       animationTime_);
}

void SkeletalMeshRenderer::CalculateNodeTransform(
	const Node&      node, const Mat4& parentTransform,
	const Animation* animation, float  animationTime) {
	Mat4 nodeTransform = node.localMat;

	// アニメーションデータがある場合は適用
	if (animation && animation->nodeAnimations.find(node.name) != animation->
		nodeAnimations.end()) {
		const NodeAnimation& nodeAnim = animation->nodeAnimations.at(node.name);

		Vec3 translation = CalculateValue(nodeAnim.translate.keyFrames,
		                                  animationTime);
		Quaternion rotation = CalculateValue(nodeAnim.rotate.keyFrames,
		                                     animationTime);

		Vec3 scale    = CalculateValue(nodeAnim.scale.keyFrames, animationTime);
		nodeTransform = Mat4::Affine(
			scale,
			rotation,
			translation
		);
	}

	// 左手座標系かつ行ベクトルの場合、変換順序を修正
	Mat4 globalTransform = nodeTransform * parentTransform;

	// このノードがボーンの場合、変換行列を設定
	if (skeletalMesh_) {
		const Skeleton& skeleton = skeletalMesh_->GetSkeleton();
		auto            it       = skeleton.boneMap.find(node.name);
		if (it != skeleton.boneMap.end()) {
			int boneIndex = it->second;
			if (boneIndex < BoneMatrices::MAX_BONES) {
				// 元の計算に戻す（補正なし）
				boneMatrices_->bones[boneIndex] = skeleton.
					bones[boneIndex].offsetMatrix * globalTransform;
			}
		}
	}

	// 子ノードを再帰的に処理
	for (const Node& child : node.children) {
		CalculateNodeTransform(child, globalTransform, animation,
		                       animationTime);
	}
}

void SkeletalMeshRenderer::DrawBoneHierarchy(
	const Node& node,
	const Mat4& parentTransform
) {
	Mat4 nodeTransform = node.localMat;

	// アニメーションデータがある場合は適用
	if (currentAnimation_ && currentAnimation_->nodeAnimations.contains(
		node.name)) {
		const NodeAnimation& nodeAnim = currentAnimation_->nodeAnimations.at(
			node.name);

		Vec3 translation = CalculateValue(nodeAnim.translate.keyFrames,
		                                  animationTime_);
		Quaternion rotation = CalculateValue(nodeAnim.rotate.keyFrames,
		                                     animationTime_);
		Vec3 scale = CalculateValue(nodeAnim.scale.keyFrames, animationTime_);

		nodeTransform = Mat4::Affine(scale, rotation,
		                             translation);
	}

	// 左手座標系かつ行ベクトルの場合、変換順序を修正
	Mat4       globalTransform = nodeTransform * parentTransform;
	Vec3       nodePos         = globalTransform.GetTranslate();
	Quaternion nodeRot         = globalTransform.ToQuaternion();

	if (skeletalMesh_->GetSkeleton().boneMap.contains(node.name)) {
		Debug::DrawSphere(
			nodePos,
			nodeRot,
			0.03f,
			{1.0f, 0.2f, 0.2f, 1.0f},
			3
		);

		if (parentTransform != Mat4::identity) {
			Mat4 parentT   = parentTransform;
			Vec3 parentPos = parentT.GetTranslate();
			Debug::DrawLine(parentPos, nodePos, {0.8f, 0.8f, 0.2f, 1.0f});
			{
				const Vec3 worldPos   = nodePos;
				Vec2       screenSize = Engine::GetViewportSize();

				const Vec3 cameraPos = CameraManager::GetActiveCamera()->
				                       GetViewMat().
				                       Inverse().GetTranslate();

				// カメラとの距離を計算
				float distance = (worldPos - cameraPos).Length();

				// カメラとの距離が一定以下の場合は描画しない
				if (distance < Math::HtoM(4.0f)) {
					return;
				}

				bool  bIsOffscreen = false;
				float outAngle     = 0.0f;

				Vec2 scrPosition = Math::WorldToScreen(
					worldPos,
					screenSize,
					false,
					0.0f,
					bIsOffscreen,
					outAngle
				);

				if (!bIsOffscreen) {
#ifdef _DEBUG
					//auto   viewport  = ImGui::GetMainViewport();
					ImVec2 screenPos = {
						Engine::GetViewportLT().x, Engine::GetViewportLT().y
					};
					ImGui::SetNextWindowPos(screenPos);
					ImGui::SetNextWindowSize({screenSize.x, screenSize.y});
					ImGui::SetNextWindowBgAlpha(0.0f); // 背景を透明にする
					ImGui::Begin("##EntityName", nullptr,
					             ImGuiWindowFlags_NoBackground |
					             ImGuiWindowFlags_NoTitleBar |
					             ImGuiWindowFlags_NoResize |
					             ImGuiWindowFlags_NoMove |
					             ImGuiWindowFlags_NoSavedSettings |
					             ImGuiWindowFlags_NoDocking |
					             ImGuiWindowFlags_NoFocusOnAppearing |
					             ImGuiWindowFlags_NoInputs |
					             ImGuiWindowFlags_NoNav
					);

					ImVec2      textPos  = {scrPosition.x, scrPosition.y};
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					float outlineSize = 1.0f;

					ImVec4 textColor = ToImVec4(Vec4::white);

					ImGuiManager::TextOutlined(
						drawList,
						textPos,
						node.name.c_str(),
						textColor,
						ToImVec4(kDebugHudOutlineColor),
						outlineSize
					);

					ImGui::End();
#endif
				}
			}
		}

		Debug::DrawAxis(nodePos, globalTransform.ToQuaternion());
	} else {
		Debug::DrawAxis(nodePos, globalTransform.ToQuaternion());
	}

	for (const Node& child : node.children) {
		DrawBoneHierarchy(child, globalTransform);
	}
}

void SkeletalMeshRenderer::DrawBoneDebug() {
	if (!skeletalMesh_) return;

	const Skeleton& skeleton = skeletalMesh_->GetSkeleton();
	DrawBoneHierarchy(skeleton.rootNode, Mat4::identity);
}
