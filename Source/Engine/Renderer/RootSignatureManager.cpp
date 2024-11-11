#include "RootSignatureManager.h"

#include <format>

#include "../Lib/Console/Console.h"


bool RootSignatureManager::CreateRootSignature(
	const std::string& name,
	const std::vector<D3D12_ROOT_PARAMETER>& rootParameters,
	const D3D12_STATIC_SAMPLER_DESC* staticSamplers,
	UINT numStaticSamplers) {
	if (rootSignatures_.contains(name)) {
		return false; // すでに存在する場合は作る必要なし
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
		static_cast<UINT>(rootParameters.size()),
		rootParameters.data(),
		numStaticSamplers,
		staticSamplers,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			Console::Print(static_cast<char*>(errorBlob->GetBufferPointer()));
		}
		return false;
	}

	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		return false;
	}

	rootSignatures_[name] = rootSignature;

	Console::Print(std::format("Complete Create RootSignature : {}\n", name), kConsoleColorCompleted);

	return true;
}
