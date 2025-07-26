#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <engine/public/postprocess/IPostProcess.h>

class SrvManager;

struct VignetteParams {
	float vignetteStrength = 0.9f;
	float vignetteRadius   = 0.0f;
	float padding[2];              // 16‑byte align
};

class PPVignette final : public IPostProcess {
public:
	PPVignette(ID3D12Device* device, SrvManager* srvManager);
	~PPVignette() override = default;

	void Update(float dt) override;
	void Execute(const PostProcessContext& ctx) override;

private:
	void Init();
	void CreateRootSignature();
	void CreatePipelineState();

	ID3D12Device*                               mDevice   = nullptr;
	SrvManager*                                 mSrvMgr   = nullptr;
	uint32_t                                    mSrvIndex = 0;
	VignetteParams                              mParams   {};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	Microsoft::WRL::ComPtr<ID3D12Resource>      mParamsCb;
};
