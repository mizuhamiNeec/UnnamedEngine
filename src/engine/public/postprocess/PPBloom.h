#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <engine/public/postprocess/IPostProcess.h>

class SrvManager;

struct BloomParams {
	float bloomStrength  = 2.0f;
	float bloomThreshold = 0.0f;
	float padding[2]; // パディング
};

class PPBloom : public IPostProcess {
public:
	PPBloom(ID3D12Device* device, SrvManager* srvManager);
	~PPBloom() override = default;

	void Init();
	void Update(float deltaTime) override;
	void Execute(const PostProcessContext& context) override;

	void SetStrength(float strength) {
		mBloomParams.bloomStrength = strength;
	}

private:
	void CreateRootSignature();
	void CreatePipelineState();

	ID3D12Device* mDevice     = nullptr;
	SrvManager*   mSrvManager = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
	Microsoft::WRL::ComPtr<ID3D12Resource>      mBloomCb;

	BloomParams mBloomParams;
	uint32_t    mSrvIndex = 0;
};
