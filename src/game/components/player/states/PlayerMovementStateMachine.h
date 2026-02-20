#pragma once

#include "IPlayerMovementState.h"
#include "../MovementComponent.h"  // MovementData, MOVEMENT_STATE の定義
#include <memory>
#include <unordered_map>

class PlayerMovementStateMachine {
public:
	PlayerMovementStateMachine();
	~PlayerMovementStateMachine() = default;

	void Init(MovementComponent* context, MovementData* data);
	void Update(float dt);
	void ChangeState(MOVEMENT_STATE newStateID);
	[[nodiscard]] MOVEMENT_STATE GetCurrentStateID() const;
	void AddState(std::shared_ptr<IPlayerMovementState> state);

private:
	MovementComponent* mContext = nullptr;
	MovementData*      mData    = nullptr;

	std::unordered_map<MOVEMENT_STATE, std::shared_ptr<IPlayerMovementState>>
	mStates;
	std::shared_ptr<IPlayerMovementState> mCurrentState;
};
