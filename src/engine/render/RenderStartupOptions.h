#pragma once

#include <cstdint>

namespace Unnamed::Render {
	/// @brief Renderer起動時のShader/PSO検証方針です。
	enum class RENDER_STARTUP_VALIDATION_POLICY : uint8_t {
		Runtime,
		Strict,
	};

	/// @brief Renderer起動時の検証オプションです。
	struct RenderStartupOptions final {
		RENDER_STARTUP_VALIDATION_POLICY validationPolicy =
			RENDER_STARTUP_VALIDATION_POLICY::Runtime;
	};

	/// @brief Renderer起動時に厳格なShader/PSO検証を行うか判定します。
	/// @param options Renderer起動オプション。
	/// @return 厳格検証を行う場合true。
	[[nodiscard]] constexpr bool IsStrictRenderStartupValidation(
		const RenderStartupOptions& options
	) noexcept {
		return options.validationPolicy ==
		       RENDER_STARTUP_VALIDATION_POLICY::Strict;
	}
}
