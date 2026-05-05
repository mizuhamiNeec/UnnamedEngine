#include "GameMovementStateMachine.h"

#include <algorithm>

#include "../movement/MovementTransitionRouter.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "GMSM";
	}

	GameMovementStateMachine::GameMovementStateMachine() = default;
	GameMovementStateMachine::~GameMovementStateMachine() = default;

	void GameMovementStateMachine::Init(ConsoleSystem* console) {
		mConsole = console;
		mHasReportedMissingConsole = false;
		if (!mConsole) {
			Error(kChannel, "コンソールシステムの取得に失敗しました。");
			return;
		}

		for (AbilityEntry& entry : mAbilities) {
			if (entry.ability) {
				entry.ability->Init(mConsole);
			}
		}
	}

	void GameMovementStateMachine::Tick(
		MovementContext& context,
		const float deltaTime
	) {
		if (!mConsole) {
			if (!mHasReportedMissingConsole) {
				Warning(
					kChannel,
					"ConsoleSystem未初期化のため状態更新をスキップしています。"
				);
				mHasReportedMissingConsole = true;
			}
			return;
		}

		IMovementMode* mode = GetMode(mCurrentModeId);
		if (!mode) {
			Warning(
				kChannel,
				"現在Mode '{}' が未登録のため更新をスキップします。",
				ToString(mCurrentModeId)
			);
			return;
		}

		context.modeState.currentMode = mCurrentModeId;
		context.modeState.previousMode = mCurrentModeId;
		context.modeState.changedThisTick = false;
		context.activeAbilityMask = mActiveAbilityMask;
		context.modeTickSuppressed = false;
		context.ResetTransitions();

		for (AbilityEntry& entry : mAbilities) {
			if (!entry.ability) {
				entry.active = false;
				continue;
			}

			const MOVEMENT_ABILITY_SLOT slot = entry.ability->GetSlot();
			const bool capabilityEnabled = context.IsCapabilityEnabled(slot);
			if (!capabilityEnabled) {
				if (entry.active) {
					entry.ability->Stop(context);
					entry.active = false;
					context.activeAbilityMask &= ~AbilitySlotToMask(slot);
				}
				continue;
			}

			if (entry.active) {
				if (!entry.ability->CanRunInMode(mCurrentModeId)) {
					entry.ability->Stop(context);
					entry.active = false;
					context.activeAbilityMask &= ~AbilitySlotToMask(slot);
					continue;
				}

				if (!entry.ability->Tick(context, deltaTime)) {
					entry.ability->Stop(context);
					entry.active = false;
					context.activeAbilityMask &= ~AbilitySlotToMask(slot);
				}
				continue;
			}

			if (!entry.ability->CanRunInMode(mCurrentModeId) ||
			    !entry.ability->CanStart(context)) {
				continue;
			}

			entry.active = entry.ability->Start(context, deltaTime);
			if (entry.active) {
				context.activeAbilityMask |= AbilitySlotToMask(slot);
			}
		}

		// Ability が mode 遷移を要求した場合は、現在 mode の Tick 前に先行適用します。
		// これにより、Jump などの入力で旧 mode の更新に上書きされる問題を防ぎます。
		MovementTransitionRequest transition = {};
		if (MovementTransitionRouter::Resolve(context, transition)) {
			ApplyModeTransition(
				context,
				transition.toMode,
				transition.reason,
				transition.source
			);
			context.ResetTransitions();
			StopAbilitiesInvalidInMode(context, mCurrentModeId);
		}

		mode = GetMode(mCurrentModeId);
		if (mode && !context.modeTickSuppressed) {
			mode->Tick(context, deltaTime);
		}

		if (MovementTransitionRouter::Resolve(context, transition)) {
			ApplyModeTransition(
				context,
				transition.toMode,
				transition.reason,
				transition.source
			);
		}

		StopAbilitiesInvalidInMode(context, mCurrentModeId);
		mActiveAbilityMask = RebuildAbilityMask();
		context.activeAbilityMask = mActiveAbilityMask;
	}

	void GameMovementStateMachine::AddMode(
		const std::shared_ptr<IMovementMode>& mode
	) {
		if (!mode) {
			return;
		}

		const size_t index = static_cast<size_t>(mode->GetModeId());
		if (index >= mModes.size()) {
			Warning(
				kChannel,
				"無効なMode '{}' の登録要求を無視しました。",
				mode->GetModeName()
			);
			return;
		}

		if (mModes[index]) {
			Warning(
				kChannel,
				"Mode '{}' は既に登録済みのため、上書きします。",
				mode->GetModeName()
			);
		}
		mModes[index] = mode;
	}

	void GameMovementStateMachine::AddAbility(
		const std::shared_ptr<IMovementAbility>& ability
	) {
		if (!ability) {
			return;
		}

		for (const AbilityEntry& entry : mAbilities) {
			if (entry.ability &&
			    entry.ability->GetSlot() == ability->GetSlot()) {
				Warning(
					kChannel,
					"Ability slot '{}' が重複登録されました。後勝ちで追加します。",
					ToString(ability->GetSlot())
				);
				break;
			}
		}

		AbilityEntry entry = {};
		entry.ability = ability;
		entry.active = false;
		if (mConsole) {
			entry.ability->Init(mConsole);
		}
		mAbilities.emplace_back(std::move(entry));
	}

	void GameMovementStateMachine::SetInitialMode(const MOVEMENT_MODE_ID modeId) {
		mCurrentModeId = modeId;
		if (!mConsole) {
			return;
		}

		if (IMovementMode* mode = GetMode(modeId)) {
			mode->Enter(mConsole);
		}
	}

	MOVEMENT_MODE_ID GameMovementStateMachine::GetCurrentModeId() const {
		return mCurrentModeId;
	}

	std::string_view GameMovementStateMachine::GetCurrentModeName() const {
		if (const IMovementMode* mode = GetMode(mCurrentModeId)) {
			return mode->GetModeName();
		}
		return ToString(mCurrentModeId);
	}

	uint64_t GameMovementStateMachine::GetActiveAbilityMask() const {
		return mActiveAbilityMask;
	}

	bool GameMovementStateMachine::IsAbilityActive(
		const MOVEMENT_ABILITY_SLOT slot
	) const {
		return (mActiveAbilityMask & AbilitySlotToMask(slot)) != 0;
	}

	void GameMovementStateMachine::ApplyModeTransition(
		MovementContext& context,
		const MOVEMENT_MODE_ID toMode,
		const std::string_view,
		const std::string_view
	) {
		if (toMode == mCurrentModeId) {
			return;
		}

		IMovementMode* nextMode = GetMode(toMode);
		if (!nextMode) {
			Warning(
				kChannel,
				"未登録Mode '{}' への遷移要求を無視しました。",
				ToString(toMode)
			);
			return;
		}

		if (IMovementMode* current = GetMode(mCurrentModeId)) {
			current->Exit();
		}

		context.modeState.previousMode = mCurrentModeId;
		mCurrentModeId = toMode;
		context.modeState.currentMode = mCurrentModeId;
		context.modeState.changedThisTick = true;
		nextMode->Enter(mConsole);
	}

	void GameMovementStateMachine::StopAbilitiesInvalidInMode(
		MovementContext& context,
		const MOVEMENT_MODE_ID modeId
	) {
		for (AbilityEntry& entry : mAbilities) {
			if (!entry.active || !entry.ability) {
				continue;
			}
			if (!entry.ability->CanRunInMode(modeId)) {
				entry.ability->Stop(context);
				entry.active = false;
				context.activeAbilityMask &= ~AbilitySlotToMask(
					entry.ability->GetSlot()
				);
			}
		}
	}

	uint64_t GameMovementStateMachine::RebuildAbilityMask() const {
		uint64_t mask = 0;
		for (const AbilityEntry& entry : mAbilities) {
			if (!entry.active || !entry.ability) {
				continue;
			}
			mask |= AbilitySlotToMask(entry.ability->GetSlot());
		}
		return mask;
	}

	IMovementMode* GameMovementStateMachine::GetMode(
		const MOVEMENT_MODE_ID modeId
	) const {
		const size_t index = static_cast<size_t>(modeId);
		if (index >= mModes.size()) {
			return nullptr;
		}
		return mModes[index].get();
	}
}
