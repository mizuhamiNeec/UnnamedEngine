#include "Material.h"

#include <d3d12shader.h>

#include <Lib/Console/Console.h>

#include <ResourceSystem/Shader/Shader.h>
#include <ResourceSystem/Pipeline/PipelineManager.h>

#include "Engine.h"
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

void Material::SetConstantBuffer(const UINT shaderRegister, ID3D12Resource* buffer) {
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
		} else {
			Console::Print("定数バッファが設定されていません: " + std::to_string(shaderRegister) + "\n",
				kConsoleColorError, Channel::ResourceSystem);
		}
	}

	// テクスチャのバインド (ルートパラメータ[1]にSRVを設定)
	if (!textures_.empty()) {
		for (const auto& [name, texture] : textures_) {
			if (texture) {
				commandList->SetGraphicsRootDescriptorTable(1, texture->GetShaderResourceView());
			} else {
				Console::Print(
					"テクスチャが設定されていません: " + name + "\n",
					kConsoleColorError,
					Channel::ResourceSystem
				);
				commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetErrorTexture()->GetShaderResourceView());
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
		// 名前はシェーダー名にする
		std::string key = shader_->GetName();

		RootSignatureDesc desc = {};

		// 定数バッファのパラメータを追加
		const auto& resourceMap = shader_->GetResourceRegisterMap();
		for (const auto& [name, registerIndex] : resourceMap) {
			if (name.find("CB_") == 0) {
				D3D12_ROOT_PARAMETER param = {};
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				param.Descriptor.ShaderRegister = registerIndex;
				param.Descriptor.RegisterSpace = 0;
				desc.parameters.push_back(param);
			}
		}

		// テクスチャのパラメータを追加
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
		for (const auto& [name, registerIndex] : resourceMap) {
			if (name.find("TEX_") == 0) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = registerIndex;
				range.RegisterSpace = 0;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges.push_back(range);
			}
		}

		// テクスチャがある場合はディスクリプタテーブルを追加
		if (!ranges.empty()) {
			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
			param.DescriptorTable.pDescriptorRanges = ranges.data();
			desc.parameters.push_back(param);

			// サンプラーの追加
			D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.ShaderRegister = 0;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			desc.samplers.push_back(samplerDesc);
		}

		// INPUT_LAYOUTフラグを設定
		desc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		rootSignature_ = RootSignatureManager2::GetOrCreateRootSignature(key, desc)->Get();

		rootSignature_ = RootSignatureManager2::GetOrCreateRootSignature(key, desc)->Get();
	}
	return rootSignature_.Get();
}

void Material::InitializeRootSignature() {
	if (!shader_) {
		Console::Print("シェーダーが設定されていません\n", kConsoleColorError, Channel::ResourceSystem);
		return;
	}

	// シェーダーリソースマップからリソースを解析
	const auto& resourceMap = shader_->GetResourceRegisterMap();

	// パラメータの収集
	std::vector<std::pair<std::string, UINT>> cbvParams;
	std::vector<std::pair<std::string, UINT>> srvParams;

	// リソースの分類
	for (const auto& [name, registerIndex] : resourceMap) {
		if (name.find("CB_") == 0) {
			cbvParams.emplace_back(name, registerIndex);
		} else if (name.find("TEX_") == 0) {
			srvParams.emplace_back(name, registerIndex);
		}
	}

	// 定数バッファのパラメータを追加
	for (const auto& [name, registerIndex] : cbvParams) {
		rootSignatureBuilder_.AddConstantBuffer(registerIndex);
		Console::Print(
			std::format("定数バッファを追加: {} (register b{})\n", name, registerIndex),
			kConsoleColorCompleted,
			Channel::ResourceSystem
		);
	}

	// テクスチャのパラメータを追加
	if (!srvParams.empty()) {
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
		for (const auto& [name, registerIndex] : srvParams) {
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = registerIndex;
			range.RegisterSpace = 0;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			ranges.push_back(range);

			Console::Print(
				std::format("テクスチャを追加: {} (register t{})\n", name, registerIndex),
				kConsoleColorCompleted,
				Channel::ResourceSystem
			);
		}
		rootSignatureBuilder_.AddDescriptorTable(ranges.data(), static_cast<UINT>(ranges.size()));

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
		rootSignatureBuilder_.AddStaticSampler(samplerDesc);
	}

	// ここでBuildを呼び出し
	rootSignatureBuilder_.Build(Engine::GetRenderer()->GetDevice());

	Console::Print(
		std::format("ルートシグネチャを初期化しました: {}\n", name_),
		kConsoleColorCompleted,
		Channel::ResourceSystem
	);
}

const std::string& Material::GetName() const { return name_; }

const std::unordered_map<std::string, Texture*>& Material::GetTextures() const { return textures_; }
