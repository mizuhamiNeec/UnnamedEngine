#pragma once
#include <runtime/core/math/Math.h>

// MovementComponent.h / MovementData の定義は MovementComponent.h に存在するため
// ここでは前方宣言のみ行い、循環インクルードを防ぐ
enum class MOVEMENT_STATE;
struct MovementData;
class MovementComponent;

class IPlayerMovementState {
public:
	virtual ~IPlayerMovementState() = default;

	/// @brief 状態の初期化
	virtual void Enter(MovementComponent* context, MovementData& data) = 0;

	/// @brief 状態の更新
	virtual void Update(MovementComponent* context, MovementData& data, float dt) = 0;

	/// @brief 状態の終了
	virtual void Exit(MovementComponent* context, MovementData& data) = 0;

	/// @brief 状態IDを取得
	[[nodiscard]] virtual MOVEMENT_STATE GetStateID() const = 0;
};
