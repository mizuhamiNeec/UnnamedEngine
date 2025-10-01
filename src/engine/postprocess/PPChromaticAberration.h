#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <engine/postprocess/IPostProcess.h>

class SrvManager;

struct CAParams {
	float strength = 0.02f; // ずらし幅
	float blend      = 0.5f;
	float padding[2]; // パディング
};

class PPChromaticAberration final : public IPostProcess {
public:
	PPChromaticAberration(ID3D12Device* device, SrvManager* srvMgr);
	~PPChromaticAberration() override = default;

	void Update(float dt) override;
	void Execute(const PostProcessContext& ctx) override;

private:
	void Init();
	void CreateRootSignature();
	void CreatePipelineState();

	ID3D12Device*                               mDevice   = nullptr;
	SrvManager*                                 mSrvMgr   = nullptr;
	uint32_t                                    mSrvIndex = 0;
	CAParams                                    mParams{};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	Microsoft::WRL::ComPtr<ID3D12Resource>      mParamsCb;
};
