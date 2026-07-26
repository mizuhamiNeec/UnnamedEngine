#pragma once
#include "engine/rhi/RhiTypes.h"

namespace Unnamed::Rhi {
	/// @brief IRhiCommandContextは、描画・dispatch・copy・resource barrierを記録するRHI command契約を定義します
	class IRhiCommandContext {
	public:
		virtual ~IRhiCommandContext() = default;

		virtual void Begin() = 0;
		virtual void End() = 0;

		virtual void TransitionBackBufferToRenderTarget() = 0;
		virtual void TransitionBackBufferToPresent() = 0;

		virtual void ClearBackBuffer(const ClearColor& color) = 0;
	};
}
