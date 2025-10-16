#pragma once
#include <unordered_map>

#include <core/UnnamedMacro.h>

#include <engine/gameframework/statemachine/Event.h>
#include <engine/gameframework/statemachine/IState.h>
#include <engine/subsystem/console/Log.h>

namespace Unnamed {
	template <class TContext>
	class StateMachine {
	public:
		static constexpr int kInvalidStateId = -1;
		static constexpr int kAny = -2;

		void AddState(StateId id, IState<TContext>* state) {
			UASSERT(mStates.count(id) == 0, "State already exists");
			mStates[id] = state;
		}

		void AddTransition(const Transition<TContext>& transition) {
			mTransitions.emplace_back(transition);
		}

		void SetInitialState(const StateId id) {
			mCurrent = id;
		}

		StateId Current() const { return mCurrent; }

		void DispatchEvent(const TContext& ctx, const Event* event) {
			for (const auto& transition : mTransitions) {
				if ((transition.from == mCurrent || transition.from == kAny) &&
					transition.guard(ctx, &event)) {
					ChangeState(ctx, transition.to, &event);
					return;
				}
			}
		}

		void Update(TContext& ctx, const float dt) {
			if (auto* state = GetState(mCurrent)) { state->Update(ctx, dt); }
		}

	private:
		IState<TContext>* GetState(StateId id) {
			auto it = mStates.find(id);
			return (it == mStates.end()) ? nullptr : it->second;
		}


		void ChangeState(TContext& ctx, StateId next, const Event* cause) {
			if (mCurrent != kInvalidStateId) {
				if (auto* state = GetState(mCurrent)) {
					state->OnExit(ctx, cause);
				}
			}

			StateId prev = mCurrent;
			mCurrent     = next;
			if (auto* state = GetState(mCurrent)) {
				DevMsg(
					"FSM",
					"Enter {} (prev={},next={})",
					state->Name(),
					prev,
					mCurrent
				);
				state->OnEnter(ctx, cause);
			} else {
				Warning(
					"FSM",
					"Null state entered id={}",
					mCurrent
				);
			}
		}

	private:
		std::unordered_map<StateId, IState<TContext>*> mStates;
		std::vector<Transition<TContext>> mTransitions;
		StateId mCurrent{kInvalidStateId};
	};
}
