#include "RootSignature2.h"

#include <cassert>

#include <Lib/Console/Console.h>

#include <UnnamedResource/Manager/RootSignatureManager2.h>

void RootSignature2::AddConstantBuffer(const UINT shaderRegister, const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	rootParameters_.push_back(param);
}

void RootSignature2::AddShaderResourceView(const UINT shaderRegister, const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	rootParameters_.push_back(param);
}

void RootSignature2::AddUnorderedAccessView(const UINT shaderRegister, const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	rootParameters_.push_back(param);
}

void RootSignature2::AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, const UINT numRanges) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ディスクリプタの範囲を設定
	descriptorTableRanges_.emplace_back(ranges, ranges + numRanges);
	param.DescriptorTable.NumDescriptorRanges = numRanges;
	param.DescriptorTable.pDescriptorRanges = descriptorTableRanges_.back().data();

	rootParameters_.push_back(param);
}

void RootSignature2::AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& samplerDesc) {
	staticSamplers_.push_back(samplerDesc);
}

void RootSignature2::Init(ID3D12Device* device, const RootSignatureDesc& desc) {
	rootSignatureDesc_.NumParameters = static_cast<UINT>(desc.parameters.size());
	rootSignatureDesc_.pParameters = desc.parameters.data();
	rootSignatureDesc_.NumStaticSamplers = static_cast<UINT>(desc.samplers.size());
	rootSignatureDesc_.pStaticSamplers = desc.samplers.data();
	rootSignatureDesc_.Flags = desc.flags;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc_, D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob, &errorBlob
	);

	if (FAILED(hr)) {
		if (errorBlob) {
			Console::Print(
				"ルートシグネチャのシリアライズに失敗しました: " + std::string(static_cast<char*>(errorBlob->GetBufferPointer())),
				kConsoleColorError,
				Channel::ResourceSystem
			);
			assert(SUCCEEDED(hr));
		}
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature_.GetAddressOf())
	);

	if (FAILED(hr)) {
		Console::Print(
			"ルートシグネチャの作成に失敗しました\n",
			kConsoleColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
	}
}

void RootSignature2::Build(ID3D12Device* device) {
	D3D12_ROOT_SIGNATURE_DESC desc;
	desc.NumParameters = static_cast<UINT>(rootParameters_.size());
	desc.pParameters = rootParameters_.data();
	desc.NumStaticSamplers = static_cast<UINT>(staticSamplers_.size());
	desc.pStaticSamplers = staticSamplers_.data();
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> serializeRootSig;
	ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializeRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (FAILED(hr)) {
		Console::Print(
			"ルートシグネチャのシリアライズに失敗しました\n",
			kConsoleColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
		return;
	}

	hr = device->CreateRootSignature(
		0,
		serializeRootSig->GetBufferPointer(),
		serializeRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature_.GetAddressOf())
	);

	if (FAILED(hr)) {
		Console::Print(
			"ルートシグネチャの作成に失敗しました\n",
			kConsoleColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
	}
}

ID3D12RootSignature* RootSignature2::Get() const {
	if (rootSignature_) {
		return rootSignature_.Get();
	}

	Console::Print(
		"ルートシグネチャが作成されていません\n",
		kConsoleColorError,
		Channel::ResourceSystem
	);
	assert(false);

	return nullptr;
}
