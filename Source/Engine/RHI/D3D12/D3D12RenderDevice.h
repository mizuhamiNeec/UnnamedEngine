#pragma once
#include <d3d12.h>

#include <RHI/Base/IRenderDevice.h>

#include <wrl/client.h>

using namespace Microsoft::WRL;

class D3D12RenderDevice final : public IRenderDevice {
public:
	D3D12RenderDevice() = default;
	~D3D12RenderDevice() override = default;

	bool Init() override;
	bool Shutdown() override;

private:
	static void EnableDebugLayer();

	ComPtr<ID3D12Device> device_;
};
