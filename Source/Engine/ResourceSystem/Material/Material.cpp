#include "Material.h"

#include <d3d12shader.h>

#include <Lib/Console/Console.h>

#include <ResourceSystem/Shader/Shader.h>
#include <ResourceSystem/Pipeline/PipelineManager.h>

#include "Engine.h"
#include "Renderer/D3D12Utils.h"
#include "ResourceSystem/RootSignature/RootSignatureManager2.h"

//-----------------------------------------------------------------------------
// なるほど完璧な作戦っスね―――ッ
// 不可能だという点に目をつぶればよぉ～～～
//-----------------------------------------------------------------------------

Material::Material(std::string name, Shader* shader) :
	name_(std::move(name)),
	shader_(shader), pipelineState_(nullptr), rootSignature_(nullptr) {
	Console::Print("マテリアルを作成しました: " + name_ + "\n", kConsoleColorCompleted, Channel::ResourceSystem);

	InitializeRootSignature();
}

void Material::SetTexture(const std::string& name, Texture* texture) {
	textures_[name] = texture;
	//UINT shaderRegister = shader_->GetResourceRegister(name);

	//D3D12_DESCRIPTOR_RANGE srvRange = {};
	//srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//srvRange.NumDescriptors = 1;
	//srvRange.BaseShaderRegister = shaderRegister;
	//srvRange.RegisterSpace = 0;
	//srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//rootSignatureBuilder_.AddDescriptorTable(&srvRange, 1);

	//if (!rootSignatureBuilder_.HasStaticSampler()) {
	//	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	//	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	//	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//	staticSampler.ShaderRegister = 0; // s0 レジスタ
	//	staticSampler.RegisterSpace = 0;
	//	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//	// 追加が必要
	//	rootSignatureBuilder_.AddStaticSampler(staticSampler);
	//}
}

void Material::qaSetConstantBuffer(const UINT shaderRegister, ID3D12Resource* buffer) {
	constantBuffers_[shaderRegister] = buffer;
	//rootSignatureBuilder_.AddConstantBuffer(shaderRegister);
}

void Material::Apply(ID3D12GraphicsCommandList* commandList) {
	if (!shader_) {
		Console::Print("シェーダが設定されていません。\n", kConsoleColorError, Channel::ResourceSystem);
		return;
	}

	// ルートシグネチャの取得と設定
	auto rootSignature = GetOrCreateRootSignature(Engine::GetRenderer()->GetDevice());
	if (!rootSignature) {
		Console::Print("ルートシグネチャが設定されていません。\n", kConsoleColorError, Channel::ResourceSystem);
		return;
	}
	commandList->SetGraphicsRootSignature(rootSignature);

	// パイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	// デフォルトのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {};
	defaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.BlendState.RenderTarget[0] = defaultRenderTargetBlendDesc;

	// ラスタライザ設定
	desc.RasterizerState = {
		.FillMode = D3D12_FILL_MODE_SOLID,
		.CullMode = D3D12_CULL_MODE_BACK,
	};

	// デプスステンシル設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.DepthStencilState = depthStencilDesc;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// その他の設定
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.pRootSignature = rootSignature;
	desc.InputLayout = Vertex::inputLayout;

	auto pso = GetOrCreatePipelineState(Engine::GetRenderer()->GetDevice(), desc);
	if (!pso) {
		Console::Print("パイプラインステートが作成されていません。\n", kConsoleColorError, Channel::ResourceSystem);
		return;
	}
	commandList->SetPipelineState(pso);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { ShaderResourceViewManager::GetDescriptorHeap().Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// 定数バッファをバインド
	for (const auto& [shaderRegister, buffer] : constantBuffers_) {
		if (buffer) {
			commandList->SetGraphicsRootConstantBufferView(shaderRegister, buffer->GetGPUVirtualAddress());
			Console::Print(
				std::format("定数バッファをバインド: (b{})\n", shaderRegister),
				kConsoleColorCompleted,
				Channel::ResourceSystem
			);
		} else {
			Console::Print("定数バッファが設定されていません: " + std::to_string(shaderRegister) + "\n",
				kConsoleColorError, Channel::ResourceSystem);
		}
	}

	// テクスチャのバインド
	if (!textures_.empty()) {
		// まず定数バッファの数をカウント
		UINT cbvCount = 0;
		for (const auto& info : shader_->GetResourceRegisterMap() | std::views::values) {
			if (info.type == D3D_SIT_CBUFFER) {
				cbvCount++;
			}
		}

		for (const auto& [name, texture] : textures_) {
			if (texture) {
				// シェーダーからレジスタ番号を取得
				const auto& resourceMap = shader_->GetResourceRegisterMap();
				auto it = resourceMap.find(name);
				if (it != resourceMap.end()) {
					const ResourceInfo& resourceInfo = it->second;
					if (resourceInfo.type == D3D_SIT_TEXTURE) {
						// テクスチャのディスクリプタテーブルは定数バッファの後に配置される
						UINT tableIndex = cbvCount;
						commandList->SetGraphicsRootDescriptorTable(
							tableIndex,
							texture->GetShaderResourceView()
						);
					}
				}
			} else {
				Console::Print(
					"テクスチャが設定されていません: " + name + "\n",
					kConsoleColorError,
					Channel::ResourceSystem
				);
				// エラーテクスチャも同じインデックスで設定
				commandList->SetGraphicsRootDescriptorTable(
					cbvCount,
					TextureManager::GetErrorTexture()->GetShaderResourceView()
				);
			}
		}
	}
}

ID3D12PipelineState* Material::GetOrCreatePipelineState(
	ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& baseDesc
) {
	if (!shader_) {
		Console::Print("シェーダーが設定されていません\n", kConsoleColorError, Channel::ResourceSystem);
		return nullptr;
	}

	// キャッシュされたパイプラインステートがあれば返す
	if (pipelineState_) {
		return pipelineState_.Get();
	}

	// パイプラインステートの作成時にキーを生成
	std::string psoKey = name_ + "_PSO";

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;

	// ルートシグネチャの設定
	auto rootSig = GetOrCreateRootSignature(device);
	if (!rootSig) {
		Console::Print("ルートシグネチャの取得に失敗しました\n", kConsoleColorError, Channel::ResourceSystem);
		return nullptr;
	}
	desc.pRootSignature = rootSig;

	// シェーダーの設定
	if (shader_->GetVertexShaderBlob()) {
		desc.VS = {
			shader_->GetVertexShaderBlob()->GetBufferPointer(),
			shader_->GetVertexShaderBlob()->GetBufferSize()
		};
	}
	if (shader_->GetPixelShaderBlob()) {
		desc.PS = {
			shader_->GetPixelShaderBlob()->GetBufferPointer(),
			shader_->GetPixelShaderBlob()->GetBufferSize()
		};
	}

	// パイプラインステートの取得または作成
	pipelineState_ = PipelineManager::GetOrCreatePipelineState(device, psoKey, desc);
	return pipelineState_.Get();
}

ID3D12RootSignature* Material::GetOrCreateRootSignature([[maybe_unused]] ID3D12Device* device) {
	if (!rootSignature_) {
		// ルートシグネチャがない場合は作成
		std::string key = shader_->GetName();

		RootSignatureDesc desc = {};

		// シェーダーリソースマップからリソースを解析
		const auto& resourceMap = shader_->GetResourceRegisterMap();

		std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;

		// リソースの分類とルートパラメータの追加
		for (const auto& [name, resourceInfo] : resourceMap) {
			if (resourceInfo.type == D3D_SIT_CBUFFER) {
				D3D12_ROOT_PARAMETER param = {};
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				// resourceInfoから取得したvisibilityを使用
				param.ShaderVisibility = resourceInfo.visibility;
				param.Descriptor.ShaderRegister = resourceInfo.bindPoint;
				param.Descriptor.RegisterSpace = 0;
				desc.parameters.push_back(param);

				Console::Print(
					std::format(
						"定数バッファを追加: {} (register b{}, visibility: {})\n",
						name,
						resourceInfo.bindPoint,
						static_cast<int>(resourceInfo.visibility)
					),
					kConsoleColorCompleted,
					Channel::ResourceSystem
				);
			} else if (resourceInfo.type == D3D_SIT_TEXTURE) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = resourceInfo.bindPoint;
				range.RegisterSpace = 0;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				srvRanges.push_back(range);

				Console::Print(
					std::format("テクスチャを追加: {} (register t{})\n", name, range.BaseShaderRegister),
					kConsoleColorCompleted,
					Channel::ResourceSystem
				);
			}
		}

		// テクスチャがある場合はディスクリプタテーブルを追加
		if (!srvRanges.empty()) {
			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(srvRanges.size());
			param.DescriptorTable.pDescriptorRanges = srvRanges.data();
			desc.parameters.push_back(param);

			// サンプラーの追加
			D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.MipLODBias = 0;
			samplerDesc.MaxAnisotropy = 0;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.MinLOD = 0.0f;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			samplerDesc.ShaderRegister = 0;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			desc.samplers.push_back(samplerDesc);
		}

		// INPUT_LAYOUTフラグを設定
		desc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		rootSignature_ = RootSignatureManager2::GetOrCreateRootSignature(key, desc)->Get();
	}
	return rootSignature_.Get();
}

void Material::InitializeRootSignature() {
	if (!shader_) {
		Console::Print("シェーダーが設定されていません\n", kConsoleColorError, Channel::ResourceSystem);
	}
}

const std::string& Material::GetName() const { return name_; }

const std::unordered_map<std::string, Texture*>& Material::GetTextures() const { return textures_; }
