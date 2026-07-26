#pragma once
#include <memory>

#include "IRhiSwapChain.h"

#include "engine/EngineConfig.h"
#include "engine/rhi/RhiTypes.h"

namespace Unnamed::Rhi {
	/// @brief IRhiDeviceは、buffer・texture・pipeline生成とcommand context取得のRHI device契約を定義します
	class IRhiDevice {
	public:
		virtual ~IRhiDevice() = default;

		virtual BACKEND_TYPE GetBackendType() const = 0;

		virtual IRhiSwapChain& GetSwapChain() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void OnResize(uint32_t width, uint32_t height) = 0;
	};

	std::unique_ptr<IRhiDevice> CreateD3D12Device(
		void*                hwnd,
		const DeviceDesc&    deviceDesc,
		const SwapChainDesc& swapChainDesc
	);
}
