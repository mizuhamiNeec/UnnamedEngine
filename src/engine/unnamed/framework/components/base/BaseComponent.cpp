#include "BaseComponent.h"

#include "../../entity/Entity.h"

#include "core/hash/StableHashBuilder.h"

#include "engine/ImGui/Icons.h"
#include "engine/world/World.h"

namespace Unnamed {
	BaseComponent::BaseComponent()  = default;
	BaseComponent::~BaseComponent() = default;

	void BaseComponent::OnAttached() {
	}

	void BaseComponent::OnDetached() {
	}

	void BaseComponent::PrePhysicsTick(float) {
	}

	void BaseComponent::OnTick(float) {
	}

	void BaseComponent::OnFrameInputTick(float) {
	}

	void BaseComponent::OnRenderTick(float, float) {
	}

	void BaseComponent::PostPhysicsTick(float) {
	}

	void BaseComponent::OnPreRender() const {
	}

	void BaseComponent::OnRender() const {
	}

	void BaseComponent::OnPostRender() const {
	}

	void BaseComponent::OnEditorTick(float) {
	}

	void BaseComponent::OnEditorRender() const {
	}

	uint32_t BaseComponent::GetIcon() const {
		return kIconQuestionMark; // デフォルトはクエスションマークアイコン
	}

	void BaseComponent::DrawInspectorImGui() {
	}

	Entity* BaseComponent::GetOwner() const {
		return mOwner;
	}

	void BaseComponent::SetOwner(Entity* owner) {
		mOwner = owner;
	}

	Scene* BaseComponent::GetScene() const noexcept {
		return mOwner ? mOwner->GetScene() : nullptr;
	}

	World* BaseComponent::GetWorld() const noexcept {
		return mOwner ? mOwner->GetWorld() : nullptr;
	}

	ConsoleSystem* BaseComponent::GetConsoleSystem() const noexcept {
		const World* world = GetWorld();
		return world ? world->GetConsoleSystem() : nullptr;
	}

	InputSystem* BaseComponent::GetInputSystem() const noexcept {
		const World* world = GetWorld();
		return world ? world->GetInputSystem() : nullptr;
	}

	AssetManager* BaseComponent::GetAssetManager() const noexcept {
		const World* world = GetWorld();
		return world ? world->GetAssetManager() : nullptr;
	}

	IDemoService* BaseComponent::GetDemoService() const noexcept {
		const World* world = GetWorld();
		return world ? World::GetDemoService() : nullptr;
	}

	AudioSystem* BaseComponent::GetAudioSystem() const noexcept {
		const World* world = GetWorld();
		return world ? world->GetAudioSystem() : nullptr;
	}

	bool BaseComponent::IsActive() const noexcept {
		return mIsActive;
	}

	void BaseComponent::SetActive(const bool isActive) noexcept {
		mIsActive = isActive;
	}

	uint64_t BaseComponent::GetGuid() const noexcept {
		return mGuid;
	}

	void BaseComponent::SetGuid(const uint64_t guid) noexcept {
		mGuid = guid;
	}

	uint64_t BaseComponent::GetTypeId() const {
		StableHashBuilder hashBuilder;
		hashBuilder.AddString(GetStableName());
		return hashBuilder.Value();
	}

	BaseComponent::TICK_GROUP BaseComponent::GetTickGroup() const {
		return TICK_GROUP::GAMEPLAY;
	}
}
