#pragma once

#include <dxgi1_6.h>
#include <memory>
#include <wrl.h>

#include <Renderer/AbstractionLayer/Base/BaseRenderer.h>
#include <d3d12.h>

class D3D12Device;

using namespace Microsoft::WRL;

class D3D12Renderer : public BaseRenderer {
public:
	D3D12Renderer();
	virtual ~D3D12Renderer() override;

	bool Init(const RendererInitInfo& info) override;
	void Shutdown() override;

	BaseDevice* GetDevice() override;

private:
	void CreateDevice();


	std::unique_ptr<D3D12Device> d3d12Device_;

	ComPtr<ID3D12Device> device_;
	ComPtr<IDXGIFactory7> dxgiFactory_;
};

