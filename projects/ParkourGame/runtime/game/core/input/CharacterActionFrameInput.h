#pragma once

#include <cstdint>

namespace Unnamed {
	/// @brief 汎用アクションのトリガー状態を保持します。
	struct ActionTriggerInput {
		bool pressed  = false;
		bool held     = false;
		bool released = false;
	};

	/// @brief 武器/ツール共通の入力状態を保持します。
	/// @details 銃以外（近接、投擲、グラップル補助装置など）にも使える汎用アクション入力です。
	struct WeaponActionInput {
		ActionTriggerInput primary   = {};
		ActionTriggerInput secondary = {};

		bool reloadPressed    = false;
		bool cycleNextPressed = false;
		bool cyclePrevPressed = false;
	};

	/// @brief キャラクターが固定ティックで消費するアクション入力パケットです。
	struct CharacterActionFrameInput {
		WeaponActionInput weapon = {};
	};

	/// @brief キャラクター固有アクション入力の受け口です。
	class ICharacterActionInputReceiver {
	public:
		virtual ~ICharacterActionInputReceiver() = default;

		virtual void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) = 0;

		virtual void ClearDeterministicActionInputQueue() = 0;
	};
}
