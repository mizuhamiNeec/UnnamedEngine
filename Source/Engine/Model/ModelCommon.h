#pragma once

class D3D12;

class ModelCommon {
public:
	void Init(D3D12* d3d12);

	D3D12* GetD3D12()const { return d3d12_; }
private:
	D3D12* d3d12_ = nullptr;
};

