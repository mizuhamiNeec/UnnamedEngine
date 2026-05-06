#include "MovementTransitionRouter.h"

namespace Unnamed {
	bool MovementTransitionRouter::Resolve(
		const MovementContext& context,
		MovementTransitionRequest& outRequest
	) {
		if (context.transitionCount == 0) {
			return false;
		}

		size_t bestIndex = 0;
		for (size_t i = 1; i < context.transitionCount; ++i) {
			const auto lhs = static_cast<uint8_t>(
				context.transitionBuffer[i].priority
			);
			const auto rhs = static_cast<uint8_t>(
				context.transitionBuffer[bestIndex].priority
			);
			if (lhs > rhs) {
				bestIndex = i;
			}
		}

		outRequest = context.transitionBuffer[bestIndex];
		return true;
	}
}
