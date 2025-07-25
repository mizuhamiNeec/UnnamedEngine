#include <engine/public/Components/MeshRenderer/SkeletalMeshRenderer.h>

#include "engine/public/Engine.h"
#include "engine/public/Camera/CameraManager.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Debug/DebugHud.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/ImGui/ImGuiUtil.h"
#include "engine/public/ResourceSystem/Shader/Shader.h"
#include "engine/public/TextureManager/TexManager.h"

struct MatParam {
	Vec4  baseColor;
	float metallic;
	float roughness;
	float padding[2];
	Vec3  emissive;
};

SkeletalMeshRenderer::~SkeletalMeshRenderer() {
	mTransformationMatrixConstantBuffer.reset();
	mBoneMatricesConstantBuffer.reset();
	mTransformationMatrix = nullptr;
	mBoneMatrices         = nullptr;
	mSkeletalMesh         = nullptr;
	mCurrentAnimation     = nullptr;
}

void SkeletalMeshRenderer::OnAttach(Entity& owner) {
	MeshRenderer::OnAttach(owner);
	mScene = mOwner->GetTransform();

	// 変換行列用の定数バッファ
	mTransformationMatrixConstantBuffer = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(TransformationMatrix),
		"SkeletalMeshTransformation"
	);
	mTransformationMatrix = mTransformationMatrixConstantBuffer->GetPtr<
		TransformationMatrix>();
	mTransformationMatrix->wvp                   = Mat4::identity;
	mTransformationMatrix->world                 = Mat4::identity;
	mTransformationMatrix->worldInverseTranspose = Mat4::identity;

	// ボーン変換行列用の定数バッファ
	mBoneMatricesConstantBuffer = std::make_unique<ConstantBuffer>(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		sizeof(BoneMatrices),
		"BoneMatrices"
	);
	mBoneMatrices = mBoneMatricesConstantBuffer->GetPtr<BoneMatrices>();

	// ボーン行列を単位行列で初期化
	for (auto& bone : mBoneMatrices->bones) {
		bone = Mat4::identity;
	}

	// TODO: 消す予定
	{
		mMatParamCBV = std::make_unique<ConstantBuffer>(
			Unnamed::Engine::GetRenderer()->GetDevice(),
			sizeof(MatParam),
			"MatParam"
		);

		mMaterialData            = mMatParamCBV->GetPtr<MatParam>();
		mMaterialData->baseColor = {0.5f, 0.5f, 0.5f, 1.0f};
		mMaterialData->metallic  = 0.25f;
		mMaterialData->roughness = 0.5f;
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

void SkeletalMeshRenderer::Update(float deltaTime) {
	if (mIsPlaying && mCurrentAnimation) {
		mAnimationTime += deltaTime * mAnimationSpeed;

		if (mIsLooping) {
			// 最後まで行ったらリピート再生
			mAnimationTime = std::fmod(mAnimationTime,
			                           mCurrentAnimation->duration);
		} else if (mAnimationTime >= mCurrentAnimation->duration) {
			// ループしない場合は停止
			mAnimationTime = mCurrentAnimation->duration;
			mIsPlaying     = false;
		}

		// ボーン変換行列を更新
		UpdateBoneMatrices();
	}
	if (mShowBoneDebug) {
		DrawBoneDebug();
	}
}

void SkeletalMeshRenderer::Render(ID3D12GraphicsCommandList* commandList) {
	// メッシュが存在しない場合は描画をスキップ
	if (!mSkeletalMesh) {
		Console::Print("SkeletalMeshRenderer::Render - メッシュがnullです\n",
		               kConTextColorError, Channel::RenderSystem);
		return;
	}

	// 現在バインドされているマテリアルを追跡
	Material* currentlyBoundMaterial = nullptr;

	// メッシュ内の各サブメッシュを描画
	for (const auto& subMesh : mSkeletalMesh->GetSubMeshes()) {
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

			// ボーン変換行列をバインド (通常はb5)
			const UINT boneMatricesRegister = material->GetShader()->
				GetResourceRegister("gBoneMatrices");
			if (boneMatricesRegister < 0xffffffff) {
				material->SetConstantBuffer(boneMatricesRegister,
				                            mBoneMatricesConstantBuffer->
				                            GetResource());
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
			std::string meshName = mSkeletalMesh
				                       ? mSkeletalMesh->GetName()
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
#ifdef _DEBUG
	// 子クラスのインスペクターUIの描画
	if (ImGui::CollapsingHeader("SkeletalMeshRenderer",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (mSkeletalMesh) {
			ImGui::Checkbox("Show Bone Debug", &mShowBoneDebug);

			if (ImGui::TreeNode("Animation")) {
				// アニメーション制御UI
				ImGui::Text("Animation Control");
				ImGui::Separator();

				// アニメーション選択
				const auto& animations = mSkeletalMesh->GetAnimations();
				if (!animations.empty()) {
					if (ImGui::BeginCombo("Animation",
					                      mCurrentAnimationName.c_str())) {
						for (const auto& [animName, anim] : animations) {
							bool isSelected = (mCurrentAnimationName ==
								animName);
							if (ImGui::Selectable(
								animName.c_str(), isSelected)) {
								PlayAnimation(animName, mIsLooping);
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					// 再生制御
					if (ImGui::Button(mIsPlaying ? "Pause" : "Play")) {
						if (mIsPlaying) {
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
					ImGui::Checkbox("Loop", &mIsLooping);
					ImGui::DragFloat("Speed", &mAnimationSpeed, 0.1f, 0.1f,
					                 5.0f);

					if (mCurrentAnimation) {
						ImGui::Text("Time: %.2f / %.2f", mAnimationTime,
						            mCurrentAnimation->duration);

						// アニメーション時間スライダー
						float normalizedTime = mCurrentAnimation->duration > 0
							                       ? mAnimationTime /
							                       mCurrentAnimation->duration
							                       : 0.0f;
						if (ImGui::SliderFloat("Progress", &normalizedTime,
						                       0.0f,
						                       1.0f)) {
							mAnimationTime = normalizedTime * mCurrentAnimation
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
				ImGui::ColorEdit4("BaseColor", &mMaterialData->baseColor.x);
				ImGui::DragFloat("Metallic", &mMaterialData->metallic, 0.01f);
				ImGui::DragFloat("Roughness", &mMaterialData->roughness, 0.01f);
				ImGui::ColorEdit3("Emissive", &mMaterialData->emissive.x);

				ImGui::Separator();

				ImGui::Text("DirectionalLight");
				ImGui::ColorEdit4("Color##Directional",
				                  &mDirectionalLightData->color.x);
				if (ImGui::DragFloat3("Direction##Directional",
				                      &mDirectionalLightData->direction.x,
				                      0.01f)) {
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
				ImGui::DragFloat3("Position##Point",
				                  &mPointLightData->position.x,
				                  0.01f);
				ImGui::DragFloat("Intensity##Point",
				                 &mPointLightData->intensity,
				                 0.01f);
				ImGui::DragFloat("Radius##Point", &mPointLightData->radius,
				                 0.01f);
				ImGui::DragFloat("Decay##Point", &mPointLightData->decay,
				                 0.01f);

				ImGui::Text("SpotLight");
				ImGui::ColorEdit4("Color##Spot", &mSpotLightData->color.x);
				ImGui::DragFloat3("Position##Spot", &mSpotLightData->position.x,
				                  0.01f);
				ImGui::DragFloat("Intensity##Spot", &mSpotLightData->intensity,
				                 0.01f);
				ImGui::DragFloat3("Direction##Spot",
				                  &mSpotLightData->direction.x,
				                  0.01f);
				ImGui::DragFloat("Distance##Spot", &mSpotLightData->distance,
				                 0.01f);
				ImGui::DragFloat("Decay##Spot", &mSpotLightData->decay, 0.01f);
				ImGui::DragFloat("CosAngle##Spot", &mSpotLightData->cosAngle,
				                 0.01f);
				ImGui::DragFloat("CosFalloff##Spot",
				                 &mSpotLightData->cosFalloffStart, 0.01f);

				ImGui::TreePop();
			}

			ImGui::Separator();

			// スケルトン情報表示
			const Skeleton& skeleton = mSkeletalMesh->GetSkeleton();
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

			ImGui::Text("Name: %s", mSkeletalMesh->GetName().c_str());

			// サブメッシュとテクスチャ情報の表示
			ImGui::Separator();
			ImGui::Text("SubMeshes and Textures:");
			for (auto& subMesh : mSkeletalMesh->GetSubMeshes()) {
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
#endif
}

SkeletalMesh* SkeletalMeshRenderer::GetSkeletalMesh() const {
	return mSkeletalMesh;
}

void SkeletalMeshRenderer::SetSkeletalMesh(SkeletalMesh* skeletalMesh) {
	mSkeletalMesh = skeletalMesh;

	// 最初のアニメーションを自動選択
	if (mSkeletalMesh && !mSkeletalMesh->GetAnimations().empty()) {
		const auto& animations = mSkeletalMesh->GetAnimations();
		const auto& firstAnim  = animations.begin();
		mCurrentAnimationName  = firstAnim->first;
		mCurrentAnimation      = &firstAnim->second;
		mAnimationTime         = 0.0f;
	}
}

void SkeletalMeshRenderer::PlayAnimation(const std::string& animationName,
                                         bool               loop) {
	if (!mSkeletalMesh) return;

	const Animation* animation = mSkeletalMesh->GetAnimation(animationName);
	if (animation) {
		mCurrentAnimation     = animation;
		mCurrentAnimationName = animationName;
		mIsLooping            = loop;
		mIsPlaying            = true;
		mAnimationTime        = 0.0f;

		Console::Print("アニメーション再生開始: " + animationName + "\n",
		               kConTextColorCompleted, Channel::RenderSystem);
	} else {
		Console::Print("アニメーションが見つかりません: " + animationName + "\n",
		               kConTextColorError, Channel::RenderSystem);
	}
}

void SkeletalMeshRenderer::StopAnimation() {
	mIsPlaying     = false;
	mAnimationTime = 0.0f;
}

void SkeletalMeshRenderer::PauseAnimation() {
	mIsPlaying = false;
}

void SkeletalMeshRenderer::ResumeAnimation() {
	if (mCurrentAnimation) {
		mIsPlaying = true;
	}
}

void SkeletalMeshRenderer::SetAnimationSpeed(float speed) {
	mAnimationSpeed = speed;
}

bool SkeletalMeshRenderer::IsAnimationPlaying() const {
	return mIsPlaying;
}

float SkeletalMeshRenderer::GetAnimationTime() const {
	return mAnimationTime;
}

void SkeletalMeshRenderer::BindTransform(
	ID3D12GraphicsCommandList* commandList) {
	commandList;
}

void SkeletalMeshRenderer::UpdateBoneMatrices() {
	if (!mSkeletalMesh || !mCurrentAnimation) return;

	const Skeleton& skeleton = mSkeletalMesh->GetSkeleton();

	// ボーン行列を単位行列で初期化
	for (int i = 0; i < BoneMatrices::MAX_BONES; ++i) {
		mBoneMatrices->bones[i] = Mat4::identity;
	}

	// ルートノードから開始して変換行列を計算
	CalculateNodeTransform(skeleton.rootNode, Mat4::identity, mCurrentAnimation,
	                       mAnimationTime);
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
	if (mSkeletalMesh) {
		const Skeleton& skeleton = mSkeletalMesh->GetSkeleton();
		auto            it       = skeleton.boneMap.find(node.name);
		if (it != skeleton.boneMap.end()) {
			int boneIndex = it->second;
			if (boneIndex < BoneMatrices::MAX_BONES) {
				// 元の計算に戻す（補正なし）
				mBoneMatrices->bones[boneIndex] = skeleton.
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
	Mat4 nodeTransform = node.localMat * mScene->GetWorldMat();

	// アニメーションデータがある場合は適用
	if (mCurrentAnimation && mCurrentAnimation->nodeAnimations.contains(
		node.name)) {
		const NodeAnimation& nodeAnim = mCurrentAnimation->nodeAnimations.at(
			node.name);

		Vec3 translation = CalculateValue(nodeAnim.translate.keyFrames,
		                                  mAnimationTime);
		Quaternion rotation = CalculateValue(nodeAnim.rotate.keyFrames,
		                                     mAnimationTime);
		Vec3 scale = CalculateValue(nodeAnim.scale.keyFrames, mAnimationTime);

		nodeTransform = Mat4::Affine(scale, rotation,
		                             translation);
	}

	// 左手座標系かつ行ベクトルの場合、変換順序を修正
	Mat4       globalTransform = nodeTransform * parentTransform;
	Vec3       nodePos         = globalTransform.GetTranslate();
	Quaternion nodeRot         = globalTransform.ToQuaternion();

	if (mSkeletalMesh->GetSkeleton().boneMap.contains(node.name)) {
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
				Vec2       screenSize = Unnamed::Engine::GetViewportSize();

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
						Unnamed::Engine::GetViewportLT().x, Unnamed::Engine::GetViewportLT().y
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

					ImVec4 textColor = ImGuiUtil::ToImVec4(Vec4::white);

					ImGuiManager::TextOutlined(
						drawList,
						textPos,
						node.name.c_str(),
						textColor,
						ImGuiUtil::ToImVec4(kDebugHudOutlineColor),
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
	if (!mSkeletalMesh) return;

	const Skeleton& skeleton = mSkeletalMesh->GetSkeleton();
	DrawBoneHierarchy(skeleton.rootNode, Mat4::identity);
}
