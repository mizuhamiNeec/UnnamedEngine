#pragma once
#include <Components/MeshRenderer/Base/MeshRenderer.h>
#include <Entity/Base/Entity.h>
#include <Renderer/ConstantBuffer.h>
#include <ResourceSystem/Mesh/SkeletalMesh.h>
#include <ResourceSystem/Material/MaterialInstance.h>
#include <Components/Animation/AnimationComponent.h>

// ボーン変換行列用の定数バッファ構造体
struct BoneMatrices {
	static constexpr int MAX_BONES = 256;
	Mat4                 bones[MAX_BONES];
};

class SkeletalMeshRenderer : public MeshRenderer {
public:
	SkeletalMeshRenderer() = default;
	virtual ~SkeletalMeshRenderer();

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;

	// 描画処理
	virtual void Render(ID3D12GraphicsCommandList* commandList) override;
	// インスペクターの描画
	virtual void DrawInspectorImGui() override;

	SkeletalMesh* GetSkeletalMesh() const;
	void          SetSkeletalMesh(SkeletalMesh* skeletalMesh);

	// アニメーション制御
	void  PlayAnimation(const std::string& animationName, bool loop = true);
	void  StopAnimation();
	void  PauseAnimation();
	void  ResumeAnimation();
	void  SetAnimationSpeed(float speed);
	bool  IsAnimationPlaying() const;
	float GetAnimationTime() const;

protected:
	void BindTransform(ID3D12GraphicsCommandList* commandList) override;
	void UpdateBoneMatrices();
	void CalculateNodeTransform(const Node&      node, const Mat4& parentTransform,
	                            const Animation* animation,
	                            float            animationTime);

	void DrawBoneHierarchy(const Node& node, const Mat4& parentTransform);
	void DrawBoneDebug();

private:
	std::unique_ptr<ConstantBuffer> transformationMatrixConstantBuffer_;
	TransformationMatrix*           transformationMatrix_ = nullptr;

	// ボーン変換行列用の定数バッファ
	std::unique_ptr<ConstantBuffer> boneMatricesConstantBuffer_;
	BoneMatrices*                   boneMatrices_ = nullptr;

	TransformComponent* transform_    = nullptr;
	SkeletalMesh*       skeletalMesh_ = nullptr;

	// アニメーション制御
	const Animation* currentAnimation_ = nullptr;
	std::string      currentAnimationName_;
	float            animationTime_  = 0.0f;
	float            animationSpeed_ = 1.0f;
	bool             isPlaying_      = false;
	bool             isLooping_      = true;

#ifdef _DEBUG
	bool showBoneDebug_ = true; // デバッグモードの場合はtrue
#else
	bool showBoneDebug_ = false; // デバッグモードではない場合はfalse
#endif

	// ライティング用の定数
	std::unique_ptr<ConstantBuffer> matparamCBV;
	MatParam*                       materialData = nullptr;

	// b1
	std::unique_ptr<ConstantBuffer> directionalLightCB;
	DirectionalLight*               directionalLightData = nullptr;

	// b2
	std::unique_ptr<ConstantBuffer> cameraCB;
	CameraForGPU*                   cameraData = nullptr;

	// b3
	std::unique_ptr<ConstantBuffer> pointLightCB;
	PointLight*                     pointLightData = nullptr;

	// b4
	std::unique_ptr<ConstantBuffer> spotLightCB;
	SpotLight*                      spotLightData = nullptr;
};
