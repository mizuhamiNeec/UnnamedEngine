

#include "engine/ResourceSystem/RootSignature/RootSignature2.h"

#include <sstream>

#include "engine/OldConsole/Console.h"
#include "engine/ResourceSystem/RootSignature/RootSignatureManager2.h"

void RootSignature2::AddConstantBuffer(const UINT shaderRegister,
                                       const D3D12_SHADER_VISIBILITY visibility,
                                       const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.ShaderVisibility          = visibility;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace  = registerSpace;
	mRootParameters.emplace_back(param);
}

void RootSignature2::AddShaderResourceView(const UINT shaderRegister,
                                           const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_SRV;
	param.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace  = registerSpace;
	mRootParameters.emplace_back(param);
}

void RootSignature2::AddUnorderedAccessView(const UINT shaderRegister,
                                            const UINT registerSpace) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_UAV;
	param.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace  = registerSpace;
	mRootParameters.emplace_back(param);
}

void RootSignature2::AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges,
                                        const UINT numRanges) {
	D3D12_ROOT_PARAMETER param;
	param.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ディスクリプタの範囲を設定
	mDescriptorTableRanges.emplace_back(ranges, ranges + numRanges);
	param.DescriptorTable.NumDescriptorRanges = numRanges;
	param.DescriptorTable.pDescriptorRanges   = mDescriptorTableRanges.back().
		data();

	mRootParameters.emplace_back(param);
}

void RootSignature2::AddStaticSampler(
	const D3D12_STATIC_SAMPLER_DESC& samplerDesc) {
	mStaticSamplers.emplace_back(samplerDesc);
}

void RootSignature2::AddRootParameter(const D3D12_ROOT_PARAMETER1& param1) {
	D3D12_ROOT_PARAMETER param = {};
	param.ParameterType        = param1.ParameterType;
	param.ShaderVisibility     = param1.ShaderVisibility;

	switch (param1.ParameterType) {
	case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
		ranges.reserve(param1.DescriptorTable.NumDescriptorRanges);

		// ディスクリプタレンジを変換
		for (UINT i = 0; i < param1.DescriptorTable.NumDescriptorRanges; i++) {
			const auto& range1 = param1.DescriptorTable.pDescriptorRanges[i];
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = range1.RangeType;
			range.NumDescriptors = range1.NumDescriptors;
			range.BaseShaderRegister = range1.BaseShaderRegister;
			range.RegisterSpace = range1.RegisterSpace;
			range.OffsetInDescriptorsFromTableStart = range1.
				OffsetInDescriptorsFromTableStart;
			ranges.emplace_back(range);
		}

		mDescriptorTableRanges.emplace_back(std::move(ranges));
		param.DescriptorTable.NumDescriptorRanges = param1.DescriptorTable.
			NumDescriptorRanges;
		param.DescriptorTable.pDescriptorRanges = mDescriptorTableRanges.back().
			data();
		break;
	}
	case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
		param.Constants.Num32BitValues = param1.Constants.Num32BitValues;
		param.Constants.ShaderRegister = param1.Constants.ShaderRegister;
		param.Constants.RegisterSpace  = param1.Constants.RegisterSpace;
		break;
	case D3D12_ROOT_PARAMETER_TYPE_CBV:
	case D3D12_ROOT_PARAMETER_TYPE_SRV:
	case D3D12_ROOT_PARAMETER_TYPE_UAV:
		param.Descriptor.ShaderRegister = param1.Descriptor.ShaderRegister;
		param.Descriptor.RegisterSpace = param1.Descriptor.RegisterSpace;
		break;
	}

	mRootParameters.emplace_back(param);
}

void RootSignature2::Init(ID3D12Device* device, const RootSignatureDesc& desc) {
	mRootSignatureDesc.NumParameters = static_cast<UINT>(desc.parameters.
		size());
	mRootSignatureDesc.pParameters       = desc.parameters.data();
	mRootSignatureDesc.NumStaticSamplers = static_cast<UINT>(desc.samplers.
		size());
	mRootSignatureDesc.pStaticSamplers = desc.samplers.data();
	mRootSignatureDesc.Flags           = desc.flags;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&mRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob, &errorBlob
	);

	if (FAILED(hr)) {
		if (errorBlob) {
			Console::Print(
				"ルートシグネチャのシリアライズに失敗しました: " + std::string(
					static_cast<char*>(errorBlob->GetBufferPointer())),
				kConTextColorError,
				Channel::ResourceSystem
			);
			assert(SUCCEEDED(hr));
		}
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	);

	if (FAILED(hr)) {
		Console::Print(
			"ルートシグネチャの作成に失敗しました\n",
			kConTextColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
	}
}

void RootSignature2::Build(ID3D12Device* device) {
	D3D12_ROOT_SIGNATURE_DESC desc;
	desc.NumParameters = static_cast<UINT>(mRootParameters.size());
	desc.pParameters = mRootParameters.data();
	desc.NumStaticSamplers = static_cast<UINT>(mStaticSamplers.size());
	desc.pStaticSamplers = mStaticSamplers.data();
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	for (const auto& param : mRootParameters) {
		std::stringstream ss;
		ss << "Root Parameter Type: " << param.ParameterType << "\n";
		Console::Print(ss.str(), kConTextColorCompleted,
		               Channel::ResourceSystem);
	}

	 Microsoft::WRL::ComPtr < ID3DBlob > serializeRootSig;
	 Microsoft::WRL::ComPtr < ID3DBlob > errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializeRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (FAILED(hr)) {
		if (errorBlob) {
			Console::Print(
				static_cast<const char*>(errorBlob->GetBufferPointer()),
				kConTextColorError,
				Channel::ResourceSystem
			);
		}
		Console::Print(
			"ルートシグネチャのシリアライズに失敗しました\n",
			kConTextColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
	}

	hr = device->CreateRootSignature(
		0,
		serializeRootSig->GetBufferPointer(),
		serializeRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	);

	if (FAILED(hr)) {
		Console::Print(
			"ルートシグネチャの作成に失敗しました\n",
			kConTextColorError,
			Channel::ResourceSystem
		);
		assert(SUCCEEDED(hr));
	}
}

ID3D12RootSignature* RootSignature2::Get() const {
	if (mRootSignature) {
		return mRootSignature.Get();
	}

	Console::Print(
		"ルートシグネチャが作成されていません\n",
		kConTextColorError,
		Channel::ResourceSystem
	);
	assert(false);

	return nullptr;
}

bool RootSignature2::HasStaticSampler() const {
	return !mStaticSamplers.empty();
}

void RootSignature2::Release() {
	if (mRootSignature) {
		mRootSignature.Reset();
	}

	mRootParameters.clear();
	mDescriptorTableRanges.clear();
	mStaticSamplers.clear();
}
