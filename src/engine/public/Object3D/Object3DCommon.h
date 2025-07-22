#pragma once

#include <memory>

#include <engine/public/renderer/PipelineState.h>
#include <engine/public/renderer/RootSignatureManager.h>

class CameraComponent;
class D3D12;

class Object3DCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Render() const;

	D3D12* GetD3D12() const;

	// Getter
	static CameraComponent* GetDefaultCamera();

private:
	CameraComponent* defaultCamera_ = nullptr;
	D3D12* d3d12_ = nullptr;
	std::unique_ptr<RootSignatureManager> rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
};
