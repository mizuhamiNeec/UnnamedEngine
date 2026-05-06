#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class InputSystem;
	class BaseCharacterComponent;

	/// @brief BaseCharacterComponentを制御するためのコントローラーの基底クラスです。
	/// @details プレイヤー、BOT、リプレイなど、様々なキャラクター制御の実装がこのクラスを継承して行われることを想定しています。
	class BaseCharacterController : public BaseComponent {
	public:
		~BaseCharacterController() override;

		// ---- BaseCharacterController ---------------------------------------


		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	protected:
		BaseCharacterComponent* mTarget = nullptr;
	};
}
