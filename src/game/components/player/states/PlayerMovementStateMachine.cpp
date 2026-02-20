#include "PlayerMovementStateMachine.h"

PlayerMovementStateMachine::PlayerMovementStateMachine() {}

void PlayerMovementStateMachine::Init(MovementComponent* context, MovementData* data) {
	mContext = context;
	mData    = data;
}

void PlayerMovementStateMachine::Update(float dt) {
	if (mCurrentState) {
		mCurrentState->Update(mContext, *mData, dt);
	}
}

void PlayerMovementStateMachine::ChangeState(MOVEMENT_STATE newStateID) {
	if (mCurrentState && mCurrentState->GetStateID() == newStateID) {
		return;
	}

	auto it = mStates.find(newStateID);
	if (it != mStates.end()) {
		if (mCurrentState) {
			mCurrentState->Exit(mContext, *mData);
		}
		mCurrentState = it->second;
		mData->state  = newStateID;
		mCurrentState->Enter(mContext, *mData);
	}
}

MOVEMENT_STATE PlayerMovementStateMachine::GetCurrentStateID() const {
	if (mCurrentState) {
		return mCurrentState->GetStateID();
	}
	// デフォルト
	return MOVEMENT_STATE::AIR;
}

void PlayerMovementStateMachine::AddState(std::shared_ptr<IPlayerMovementState> state) {
	if (state) {
		mStates[state->GetStateID()] = state;
	}
}
