#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include <engine/public/postprocess/IPostProcess.h>

class SrvManager;
class ShaderResourceViewManager;

class CopyImagePass : public IPostProcess {
public:
	CopyImagePass(ID3D12Device* device, SrvManager* srvManager);
	~CopyImagePass();

	void Init();

	void Update(float deltaTime) override;

	void Execute(const PostProcessContext& context) override;

	void Shutdown() {
	}

	// SRVインデックスを取得するメソッドを追加
	uint32_t GetSrvIndex() const { return mSrvIndex; }

private:
	void CreateRootSignature();
	void CreatePipelineState();

	SrvManager* mSrvManager = nullptr;
	uint32_t    mSrvIndex   = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
	ID3D12Device*                               mDevice;
};
