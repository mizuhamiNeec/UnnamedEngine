#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <engine/public/postprocess/IPostProcess.h>

class SrvManager;

// ── パラメータ（CBV）──────────────────────
struct RadialBlurParams {
	float blurStrength = 0.2f;  // 0=無効, 1=最大
	float blurRadius   = 0.5f; // 0.0=center, 1.0=edge
	float padding[2];
};

// ── ポストプロセスクラス ────────────────
class PPRadialBlur final : public IPostProcess {
public:
	PPRadialBlur(ID3D12Device* device, SrvManager* srvMgr);
	~PPRadialBlur() override = default;

	void Update(float dt) override;
	void Execute(const PostProcessContext& ctx) override;

	void SetBlurStrength(float strength) {
		mParams.blurStrength = strength;
	}

private:
	void Init();
	void CreateRootSignature();
	void CreatePipelineState();

	ID3D12Device* mDevice   = nullptr;
	SrvManager*   mSrvMgr   = nullptr;
	uint32_t      mSrvIndex = 0;

	RadialBlurParams                            mParams{};
	Microsoft::WRL::ComPtr<ID3D12Resource>      mParamsCb;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPso;
};
