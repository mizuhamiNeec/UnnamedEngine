#pragma once
#include "Animation/Animation.h"

#include "Components/Base/Component.h"

class TransformComponent;

class AnimationComponent : public Component {
public:
	~AnimationComponent() override;

	explicit AnimationComponent(Animation animation);

	void SetPlaying(const bool isPlaying) { bIsPlaying_ = isPlaying; }
	void SetLooping(const bool isLooping) { bIsLooping_ = isLooping; }

	void OnAttach(Entity& owner) override;
	void OnDetach() override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;
	void DrawInspectorImGui() override;

	[[nodiscard]] bool    IsEditorOnly() const override { return false; }
	[[nodiscard]] Entity* GetOwner() const override { return owner_; }

private:
	Animation animation_;
	std::string              currentNodeName_; // 現在のノード名

	bool bIsAutoPlay_ = false; // スポーン時に再生するか?
	bool bIsPlaying_  = false; // 再生中か?
	bool bIsLooping_  = false; // ループ再生するか?

	float animationTime = 0.0f; // アニメーションの現在の時間

	TransformComponent* transform_ = nullptr;
};
