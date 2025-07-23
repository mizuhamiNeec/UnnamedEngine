#pragma once
#include <engine/public/Components/Base/Component.h>
#include <engine/public/Animation/Animation.h>

class SceneComponent;

class AnimationComponent : public Component {
public:
	~AnimationComponent() override;

	explicit AnimationComponent(Animation animation);

	void SetPlaying(const bool isPlaying) { mIsPlaying = isPlaying; }
	void SetLooping(const bool isLooping) { mIsLooping = isLooping; }

	void OnAttach(Entity& owner) override;
	void OnDetach() override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;
	void DrawInspectorImGui() override;

	[[nodiscard]] bool    IsEditorOnly() const override { return false; }
	[[nodiscard]] Entity* GetOwner() const override { return mOwner; }

private:
	Animation   mAnimation;
	std::string mCurrentNodeName; // 現在のノード名

	bool mIsAutoPlay = false; // スポーン時に再生するか?
	bool mIsPlaying  = false; // 再生中か?
	bool mIsLooping  = false; // ループ再生するか?

	float mAnimationTime = 0.0f; // アニメーションの現在の時間
};
