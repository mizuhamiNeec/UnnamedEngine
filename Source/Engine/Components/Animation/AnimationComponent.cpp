#include <Components/Animation/AnimationComponent.h>

#include <Entity/Base/Entity.h>

AnimationComponent::~AnimationComponent() {
}

AnimationComponent::AnimationComponent(
	Animation animation
) : animation_(std::move(animation)) {
	// アニメーションのノード名を初期化 初期は最初のノード名を設定
	currentNodeName_ = animation_.nodeNames.empty()
		                   ? ""
		                   : animation_.nodeNames[0];
}

void AnimationComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	transform_ = owner.GetTransform();
	if (bIsAutoPlay_) {
		bIsPlaying_ = true;
	}
}

void AnimationComponent::OnDetach() {
	Component::OnDetach();
}

void AnimationComponent::Update(const float deltaTime) {
	if (bIsPlaying_) {
		animationTime += deltaTime;
		if (bIsLooping_) {
			// 最後まで行ったらリピート再生
			animationTime = std::fmod(animationTime, animation_.duration);
		}
		
		// 現在選択されているノードのアニメーションを再生
		if (!currentNodeName_.empty() && 
		    animation_.nodeAnimations.find(currentNodeName_) != animation_.nodeAnimations.end()) {
			
			NodeAnimation& nodeAnimation = animation_.nodeAnimations[currentNodeName_];
			
			Vec3 translate = CalculateValue(
				nodeAnimation.translate.keyFrames,
				animationTime
			);
			Quaternion rotate = CalculateValue(
				nodeAnimation.rotate.keyFrames,
				animationTime
			);
			Vec3 scale = CalculateValue(
				nodeAnimation.scale.keyFrames,
				animationTime
			);

			// 適用
			transform_->SetLocalPos(translate);
			transform_->SetLocalRot(rotate);
			transform_->SetLocalScale(scale);
		}
		// デフォルトのフォールバック（既存のコード）
		else if (animation_.nodeAnimations.find("AnimatedCube") != animation_.nodeAnimations.end()) {
			NodeAnimation& rootNodeAnimation = animation_.nodeAnimations["AnimatedCube"];
			Vec3 translate = CalculateValue(
				rootNodeAnimation.translate.keyFrames,
				animationTime
			);
			Quaternion rotate = CalculateValue(
				rootNodeAnimation.rotate.keyFrames,
				animationTime
			);
			Vec3 scale = CalculateValue(
				rootNodeAnimation.scale.keyFrames,
				animationTime
			);

			// 適用
			transform_->SetLocalPos(translate);
			transform_->SetLocalRot(rotate);
			transform_->SetLocalScale(scale);
		}
	}
}

void AnimationComponent::Render(ID3D12GraphicsCommandList* commandList) {
	Component::Render(commandList);
}

void AnimationComponent::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader("Animation Component")) {
		if (ImGui::BeginCombo("NodeName", currentNodeName_.c_str())) {
			for (const auto& nodeName : animation_.nodeNames) {
				bool isSelected = (currentNodeName_ == nodeName);
				if (ImGui::Selectable(nodeName.c_str(), isSelected)) {
					currentNodeName_ = nodeName;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Checkbox("Auto Play", &bIsAutoPlay_);
		ImGui::Checkbox("Playing", &bIsPlaying_);
		ImGui::Checkbox("Looping", &bIsLooping_);
		ImGui::Text("Animation Time: %.2f", animationTime);
	}
}
