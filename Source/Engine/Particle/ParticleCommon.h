#pragma once

#include "../Renderer/PipelineState.h"

#include "../Renderer/SrvManager.h"

class Camera;
class RootSignatureManager;
class D3D12;

class ParticleCommon {
public:
	void Init(D3D12* d3d12, SrvManager* srvManager);
	void Shutdown() const;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Render();

	D3D12* GetD3D12() const;

	// Setter
	void SetDefaultCamera(Camera* newCamera);

	// Getter
	Camera* GetDefaultCamera() const;
	SrvManager* GetSrvManager() const;

private:
	D3D12* d3d12_ = nullptr;
	Camera* defaultCamera_ = nullptr;
	RootSignatureManager* rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
	SrvManager* srvManager_ = nullptr;
};
