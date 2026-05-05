#pragma once

#include "MovementTypes.h"

namespace Unnamed {
	/// @brief 遷移要求バッファから最終遷移先を解決します。
	class MovementTransitionRouter {
	public:
		/// @brief 最も優先度の高い遷移要求を解決します。
		/// @return 遷移が決定した場合true
		static bool Resolve(
			const MovementContext& context,
			MovementTransitionRequest& outRequest
		);
	};
}
