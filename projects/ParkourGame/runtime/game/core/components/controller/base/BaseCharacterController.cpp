#include "BaseCharacterController.h"

#include "../../character/base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"

namespace Unnamed {
	BaseCharacterController::~BaseCharacterController() = default;

	void BaseCharacterController::OnAttached() {
		BaseComponent::OnAttached();

		// BaseCharacterComponent基底のコンポーネントを探す
		if (
			auto* character =
				mOwner->FindComponentByBase<BaseCharacterComponent>()
		) {
			mTarget = character;
			return;
		}
		Error(
			GetComponentName(),
			"対象のBaseCharacterComponentが見つかりませんでした。BaseCharacterControllerは正しく動作しません。"
		);
	}

	void BaseCharacterController::OnDetached() {
		BaseComponent::OnDetached();
		mTarget = nullptr;
	}

	std::string_view BaseCharacterController::GetStableName() const {
		return "game.BaseCharacterController";
	}

	std::string_view BaseCharacterController::GetComponentName() const {
		return "BaseCharacterController";
	}

	uint32_t BaseCharacterController::GetIcon() const {
		return kIconJoystick;
	}

#ifdef _DEBUG
	void BaseCharacterController::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();
	}
#endif

	void BaseCharacterController::Deserialize(const JsonReader&) {}

	void BaseCharacterController::Serialize(JsonWriter&) const {}

	REGISTER_COMPONENT(BaseCharacterController);
}
