#pragma once
#include <cstdint>

#include <engine/rhi/RhiTypes.h>

namespace Unnamed::Rhi {
	/// @brief IRhiSwapChainは、back buffer取得、resize、presentを提供するswap chain契約を定義します
	class IRhiSwapChain {
	public:
		virtual ~IRhiSwapChain() = default;

		[[nodiscard]] virtual uint32_t GetWidth() const = 0;
		[[nodiscard]] virtual uint32_t GetHeight() const = 0;
		[[nodiscard]] virtual uint32_t GetBufferCount() const = 0;

		[[nodiscard]] virtual uint32_t GetCurrentBackBufferIndex() const = 0;

		[[nodiscard]] virtual TEXTURE_FORMAT GetFormat() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void Present(bool vSync) = 0;
	};
}
