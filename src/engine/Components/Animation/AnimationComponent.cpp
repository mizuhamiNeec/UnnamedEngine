#include <engine/Animation/Animation.h>
#include <engine/Components/Animation/AnimationComponent.h>
#include <engine/Entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

/// @brief アニメーションコンポーネントのデストラクタ
AnimationComponent::~AnimationComponent() {
}

/// @brief アニメーションコンポーネントのコンストラクタ
/// @param animation アニメーションデータ
AnimationComponent::AnimationComponent(
	Animation animation
) : mAnimation(std::move(animation)) {
	// アニメーションのノード名を初期化 初期は最初のノード名を設定
	mCurrentNodeName = mAnimation.nodeNames.empty() ?
		                   "" :
		                   mAnimation.nodeNames[0];
}

/// @brief エンティティにアタッチされたときに呼ばれる
/// @param owner 所有するエンティティ
void AnimationComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mScene = owner.GetTransform();
	if (mIsAutoPlay) {
		mIsPlaying = true;
	}
}

/// @brief エンティティからデタッチされたときに呼ばれる
void AnimationComponent::OnDetach() {
	Component::OnDetach();
}

/// @brief アニメーションコンポーネントの更新
/// @param deltaTime 前フレームからの経過時間
void AnimationComponent::Update(const float deltaTime) {
	if (mIsPlaying) {
		mAnimationTime += deltaTime;
		if (mIsLooping) {
			// 最後まで行ったらリピート再生
			mAnimationTime = std::fmod(mAnimationTime, mAnimation.duration);
		}

		// 現在選択されているノードのアニメーションを再生
		if (!mCurrentNodeName.empty() &&
			mAnimation.nodeAnimations.contains(mCurrentNodeName)) {
			NodeAnimation& nodeAnimation = mAnimation.nodeAnimations[
				mCurrentNodeName];

			const Vec3 translate = CalculateValue(
				nodeAnimation.translate.keyFrames,
				mAnimationTime
			);
			const Quaternion rotate = CalculateValue(
				nodeAnimation.rotate.keyFrames,
				mAnimationTime
			);
			const Vec3 scale = CalculateValue(
				nodeAnimation.scale.keyFrames,
				mAnimationTime
			);

			// 適用
			mScene->SetLocalPos(translate);
			mScene->SetLocalRot(rotate);
			mScene->SetLocalScale(scale);
		}
		// デフォルトのフォールバック（既存のコード）
		else if (mAnimation.nodeAnimations.contains("AnimatedCube")) {
			NodeAnimation& rootNodeAnimation = mAnimation.nodeAnimations[
				"AnimatedCube"];
			Vec3 translate = CalculateValue(
				rootNodeAnimation.translate.keyFrames,
				mAnimationTime
			);
			Quaternion rotate = CalculateValue(
				rootNodeAnimation.rotate.keyFrames,
				mAnimationTime
			);
			Vec3 scale = CalculateValue(
				rootNodeAnimation.scale.keyFrames,
				mAnimationTime
			);

			// 適用
			mScene->SetLocalPos(translate);
			mScene->SetLocalRot(rotate);
			mScene->SetLocalScale(scale);
		}
	}
}

/// @brief アニメーションコンポーネントのレンダリング
/// @param commandList コマンドリスト
void AnimationComponent::Render(ID3D12GraphicsCommandList* commandList) {
	Component::Render(commandList);
}

/// @brief ImGuiでインスペクターを描画
void AnimationComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Animation Component")) {
		if (ImGui::BeginCombo("NodeName", mCurrentNodeName.c_str())) {
			for (const auto& nodeName : mAnimation.nodeNames) {
				bool isSelected = (mCurrentNodeName == nodeName);
				if (ImGui::Selectable(nodeName.c_str(), isSelected)) {
					mCurrentNodeName = nodeName;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Checkbox("Auto Play", &mIsAutoPlay);
		ImGui::Checkbox("Playing", &mIsPlaying);
		ImGui::Checkbox("Looping", &mIsLooping);
		ImGui::Text("Animation Time: %.2f", mAnimationTime);
	}
#endif
}
