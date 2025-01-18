#include "Material.h"

#include <UnnamedResource/Shader.h>

#include "Texture.h"

#include "Lib/Console/Console.h"

#include "Manager/PipelineManager.h"

Material::Material(std::string name, Shader* shader) :
	name_(std::move(name)),
	shader_(shader), pipelineState_(nullptr), rootSignature_(nullptr) {
	Console::Print("マテリアルを作成しました: " + name_ + "\n", kConsoleColorCompleted, Channel::ResourceSystem);
}

void Material::SetTexture(const std::string& name, Texture* texture) {
	textures_[name] = texture;

	UINT shaderRegister = shader_->GetResourceRegister(name);

	// ルートシグネチャにテクスチャを登録
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = shaderRegister;
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootSignatureBuilder_.AddDescriptorTable(&srvRange, 1);
}

void Material::SetConstantBuffer(const UINT shaderRegister, ID3D12Resource* buffer) {
	constantBuffers_[shaderRegister] = buffer;
	rootSignatureBuilder_.AddConstantBuffer(shaderRegister);
}

ID3D12PipelineState* Material::GetOrCreatePipelineState(
	ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& baseDesc
) {
	// 作成していなかったら作成
	if (!pipelineState_) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;

		if (shader_->GetVertexShaderBlob()) {
			desc.VS = {
				.pShaderBytecode = shader_->GetVertexShaderBlob()->GetBufferPointer(),
				.BytecodeLength = shader_->GetVertexShaderBlob()->GetBufferSize()
			};
		}
		if (shader_->GetPixelShaderBlob()) {
			desc.PS = {
				 .pShaderBytecode = shader_->GetPixelShaderBlob()->GetBufferPointer(),
				.BytecodeLength = shader_->GetPixelShaderBlob()->GetBufferSize()
			};
		}
		if (shader_->GetGeometryShaderBlob()) {
			desc.GS = {
				.pShaderBytecode = shader_->GetGeometryShaderBlob()->GetBufferPointer(),
				.BytecodeLength = shader_->GetGeometryShaderBlob()->GetBufferSize()
			};
		}

		// 末尾に"_PSO"をつけてパイプラインステートを作成
		pipelineState_ = PipelineManager::GetOrCreatePipelineState(device, name_ + "_PSO", desc);
	}

	return pipelineState_.Get();
}

ID3D12RootSignature* Material::GetOrCreateRootSignature(ID3D12Device* device) {
	// 作成していなかったら作成
	if (!rootSignature_) {
		// RootSignatureの作成
		rootSignatureBuilder_.Build(device);
		rootSignature_ = rootSignatureBuilder_.Get();
		Console::Print(
			"ルートシグネチャを作成しました: " + name_ + "\n",
			kConsoleColorCompleted,
			Channel::ResourceSystem
		);
	}

	return rootSignature_.Get();
}

const std::string& Material::GetName() const { return name_; }

const std::unordered_map<std::string, Texture*>& Material::GetTextures() const { return textures_; }
