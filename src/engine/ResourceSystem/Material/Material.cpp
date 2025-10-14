//-----------------------------------------------------------------------------
// なるほど完璧な作戦っスね―――ッ
// 不可能だという点に目をつぶればよぉ～～～
//-----------------------------------------------------------------------------

#include <engine/Engine.h>
#include <engine/OldConsole/Console.h>
#include <engine/renderer/SrvManager.h>
#include <engine/ResourceSystem/Material/Material.h>
#include <engine/ResourceSystem/Pipeline/PipelineManager.h>
#include <engine/ResourceSystem/RootSignature/RootSignatureManager2.h>
#include <engine/ResourceSystem/Shader/Shader.h>
#include <engine/TextureManager/TexManager.h>

Material::Material(std::string name, Shader* shader) :
	mName(std::move(name)),
	mShader(shader), mPipelineState(nullptr), mRootSignature(nullptr) {
	Console::Print("マテリアルを作成しました: " + mName + "\n", kConTextColorCompleted,
	               Channel::ResourceSystem);

	InitializeRootSignature();
}

void Material::SetTexture(const std::string& name,
                          const std::string& filePath) {
	// テクスチャを読み込み
	if (!filePath.empty()) {
		mTextures[name] = filePath;

		// デバッグログ出力
		Console::Print(
			std::format(
				"Material::SetTexture: マテリアル {} のスロット {} にテクスチャ {} を設定しました\n",
				mName, name, filePath),
			kConTextColorCompleted,
			Channel::ResourceSystem
		);
	} else {
		Console::Print(
			std::format(
				"Material::SetTexture: マテリアル {} のスロット {} に空のテクスチャパスが指定されました\n",
				mName, name),
			kConTextColorError,
			Channel::ResourceSystem
		);
	}
}

void Material::SetConstantBuffer(const UINT      shaderRegister,
                                 ID3D12Resource* buffer) {
	// シェーダーからリソース情報を取得
	for (const auto& [bindPoint, visibility, type] : mShader->
	     GetResourceRegisterMap() | std::views::values) {
		if (type == D3D_SIT_CBUFFER && bindPoint == shaderRegister) {
			// シェーダーステージとレジスタ番号からキーを生成
			std::string key       = GenerateBufferKey(visibility, bindPoint);
			mConstantBuffers[key] = buffer;
		}
	}
}

void Material::Apply(ID3D12GraphicsCommandList* commandList,
                     const std::string&         meshName) {
	if (!mShader) {
		Console::Print("シェーダが設定されていません。\n", kConTextColorError,
		               Channel::ResourceSystem);
		return;
	}

	// ルートシグネチャの取得と設定
	auto rootSignature = GetOrCreateRootSignature(
		Unnamed::Engine::GetRenderer()->GetDevice());
	if (!rootSignature) {
		Console::Print("ルートシグネチャが設定されていません。\n", kConTextColorError,
		               Channel::ResourceSystem);
		return;
	}
	commandList->SetGraphicsRootSignature(rootSignature);

	// パイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	// デフォルトのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {};
	defaultRenderTargetBlendDesc.RenderTargetWriteMask          =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.BlendState.RenderTarget[0] = defaultRenderTargetBlendDesc;

	// ラスタライザ設定
	desc.RasterizerState = {
		.FillMode = D3D12_FILL_MODE_SOLID,
		.CullMode = D3D12_CULL_MODE_BACK,
		.DepthClipEnable = TRUE,
	};

	// デプスステンシル設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable              = TRUE;
	depthStencilDesc.DepthWriteMask           = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc                = D3D12_COMPARISON_FUNC_LESS;
	desc.DepthStencilState                    = depthStencilDesc;
	desc.DSVFormat                            = DXGI_FORMAT_D32_FLOAT;

	// その他の設定
	desc.NumRenderTargets      = 1;
	desc.RTVFormats[0]         = kBufferFormat;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count      = 1;
	desc.SampleMask            = D3D12_DEFAULT_SAMPLE_MASK;
	desc.pRootSignature        = rootSignature;

	// シェーダー名に基づいて適切な入力レイアウトを選択
	if (mShader->GetName() == "DefaultSkinnedShader") {
		desc.InputLayout = SkinnedVertex::inputLayout;
	} else {
		desc.InputLayout = Vertex::inputLayout;
	}

	auto pso = GetOrCreatePipelineState(
		Unnamed::Engine::GetRenderer()->GetDevice(),
		desc, meshName);
	if (!pso) {
		Console::Print("パイプラインステートが作成されていません。\n", kConTextColorError,
		               Channel::ResourceSystem);
		return;
	}
	commandList->SetPipelineState(pso);

	/*Console::Print(
		"マテリアルを適用しました: " + name_ + "\n",
		kConTextColorCompleted,
		Channel::ResourceSystem
	);*/

	// 定数バッファをバインド
	UINT parameterIndex = 0; // ルートパラメータのインデックスを追跡
	for (const auto& [bindPoint, visibility, type] : mShader->
	     GetResourceRegisterMap() | std::views::values) {
		if (type == D3D_SIT_CBUFFER) {
			// キーを生成
			std::string key = GenerateBufferKey(visibility, bindPoint);

			// 対応する定数バッファを検索
			auto it = mConstantBuffers.find(key);
			if (it != mConstantBuffers.end() && it->second) {
				commandList->SetGraphicsRootConstantBufferView(
					parameterIndex,
					it->second->GetGPUVirtualAddress()
				);
			}
			parameterIndex++;
		}
	}

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* descriptorHeaps[] = {
		Unnamed::Engine::GetSrvManager()->GetDescriptorHeap()
	};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// テクスチャのディスクリプタテーブルバインド
	if (!mTextures.empty()) {
		// まず定数バッファの数をカウント
		UINT cbvCount = 0;
		for (const auto& info : mShader->GetResourceRegisterMap() |
		     std::views::values) {
			if (info.type == D3D_SIT_CBUFFER) {
				cbvCount++;
			}
		}

		// テクスチャのディスクリプタテーブルは全てのCBVの後
		//		UINT tableIndex = cbvCount;

		// シェーダーレジスタ順序（t0, t1, t2, ...）でテクスチャを並べる
		std::map<UINT, std::pair<std::string, std::string>> texturesByRegister;

		for (const auto& [name, filePath] : mTextures) {
			if (!filePath.empty()) {
				const auto& resourceMap = mShader->GetResourceRegisterMap();
				auto        it          = resourceMap.find(name);
				if (it != resourceMap.end()) {
					const ResourceInfo& resourceInfo = it->second;
					if (resourceInfo.type == D3D_SIT_TEXTURE) {
						texturesByRegister[resourceInfo.bindPoint] = {
							name, filePath
						};
					}
				} else {
					// Console::Print(
					// 	std::format(
					// 		"Material::Apply: 警告 - シェーダーにテクスチャ {} が見つかりません\n",
					// 		name),
					// 	kConTextColorWarning,
					// 	Channel::ResourceSystem
					// );
				}
			}
		}

		// テクスチャの合計数を計算
		UINT totalTextureCount = static_cast<UINT>(texturesByRegister.size());
		
		if (totalTextureCount > 0) {
			auto srvManager = Unnamed::Engine::GetSrvManager();
			auto texManager = TexManager::GetInstance();
			
			// テクスチャの組み合わせキーを生成（再利用判定用）
			std::string textureComboKey;
			for (const auto& [registerIndex, textureInfo] : texturesByRegister) {
				const auto& [textureName, filePath] = textureInfo;
				textureComboKey += filePath + "|";
			}
			
			// 既存の連続SRVスロットが使用可能かチェック
			static std::map<std::string, uint32_t> textureCombinationCache;
			uint32_t consecutiveStartIndex = 0;
			bool reusingExisting = false;
			
			auto cacheIt = textureCombinationCache.find(textureComboKey);
			if (cacheIt != textureCombinationCache.end()) {
				// 既存の組み合わせを再利用
				consecutiveStartIndex = cacheIt->second;
				reusingExisting = true;
			} else {
				// 新しい連続スロットを確保
				consecutiveStartIndex = srvManager->AllocateConsecutiveTexture2DSlots(totalTextureCount);
				if (consecutiveStartIndex != 0) {
					textureCombinationCache[textureComboKey] = consecutiveStartIndex;
				}
			}
			
			if (consecutiveStartIndex != 0) {
				// シェーダーの期待に合わせてテクスチャを配置
				// t0 = Texture2D (BaseColor), t1 = TextureCube (Environment)
				std::vector<std::pair<UINT, std::pair<std::string, std::string>>> texture2DList;
				std::vector<std::pair<UINT, std::pair<std::string, std::string>>> textureCubeList;
				
				// テクスチャタイプ別に分類
				for (const auto& [registerIndex, textureInfo] : texturesByRegister) {
					const auto& [textureName, filePath] = textureInfo;
					auto textureData = texManager->GetTextureData(filePath);
					
					if (textureData) {
						bool isCubeMap = (textureData->metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0;
						
						if (isCubeMap) {
							textureCubeList.push_back({registerIndex, textureInfo});
						} else {
							texture2DList.push_back({registerIndex, textureInfo});
						}
					}
				}
				
				// Nsightでのインデックスズレを考慮して逆順で配置を試行
				// TextureCube -> Texture2D の順で配置（通常とは逆）
				std::vector<std::pair<UINT, std::pair<std::string, std::string>>> nsightOrderedTextures;
				
				// t1にTextureCubeを配置（インデックス0）
				for (const auto& item : textureCubeList) {
					nsightOrderedTextures.push_back(item);
				}
				// t0にTexture2Dを配置（インデックス1）
				for (const auto& item : texture2DList) {
					nsightOrderedTextures.push_back(item);
				}
				
				// 各テクスチャを連続したスロットに配置（Nsight観察順）
				// キャッシュされた組み合わせの場合はSRV作成をスキップ
				if (!reusingExisting) {
					for (size_t i = 0; i < nsightOrderedTextures.size(); ++i) {
						const auto& [registerIndex, textureInfo] = nsightOrderedTextures[i];
						const auto& [textureName, filePath] = textureInfo;
						uint32_t newSrvIndex = consecutiveStartIndex + static_cast<uint32_t>(i);
						
						// テクスチャタイプを再確認
						auto textureData = texManager->GetTextureData(filePath);
						if (textureData) {
							bool isCubeMap = (textureData->metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0;
							
							// 既存のテクスチャリソースを取得
							auto textureResource = texManager->GetTextureResource(filePath);
							if (textureResource) {
								// 連続スロットに適切なSRVを作成
								if (isCubeMap) {
									srvManager->CreateSRVForTextureCube(newSrvIndex, textureResource, DXGI_FORMAT_UNKNOWN, 1);
								} else {
									srvManager->CreateSRVForTexture2D(newSrvIndex, textureResource.Get(), DXGI_FORMAT_UNKNOWN, 1);
								}
								
								// TexManagerのマッピングを更新
								texManager->UpdateTextureSrvIndex(filePath, newSrvIndex);
							}
						}
					}
				} else {
					// キャッシュ再利用の場合、TexManagerのマッピングだけ更新
					for (size_t i = 0; i < nsightOrderedTextures.size(); ++i) {
						const auto& [registerIndex, textureInfo] = nsightOrderedTextures[i];
						const auto& [textureName, filePath] = textureInfo;
						uint32_t newSrvIndex = consecutiveStartIndex + static_cast<uint32_t>(i);
						
						// TexManagerのマッピングを更新
						texManager->UpdateTextureSrvIndex(filePath, newSrvIndex);
					}
				}
				
				// ルートシグネチャの構造に基づいてテクスチャディスクリプタテーブルのインデックスを計算
				const auto& resourceMap = mShader->GetResourceRegisterMap();
				UINT constantBufferCount = 0;
				for (const auto& [name, resourceInfo] : resourceMap) {
					if (resourceInfo.type == D3D_SIT_CBUFFER) {
						constantBufferCount++;
					}
				}
				
				// テクスチャディスクリプタテーブルは定数バッファの後
				// （定数バッファ数に関係なく、すべての定数バッファの後にディスクリプタテーブルが配置される）
				UINT textureTableRootParameterIndex = constantBufferCount;
				
				srvManager->SetGraphicsRootDescriptorTable(textureTableRootParameterIndex, consecutiveStartIndex);
			} else {
				Console::Print(
					std::format(
						"Material::Apply: エラー - {}個の連続したテクスチャスロットを確保できませんでした\n",
						totalTextureCount),
					kConTextColorError,
					Channel::ResourceSystem
				);
			}
		}
	}
}

ID3D12PipelineState* Material::GetOrCreatePipelineState(
	ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& baseDesc,
	const std::string& meshName
) {
	if (!mShader) {
		Console::Print("シェーダーが設定されていません\n", kConTextColorError,
		               Channel::ResourceSystem);
		return nullptr;
	}

	// キャッシュされたパイプラインステートがあれば返す
	if (mPipelineState) {
		return mPipelineState.Get();
	}

	// パイプラインステートの作成時にキーを生成：メッシュ名_マテリアル名_PSO
	std::string psoKey;
	if (!meshName.empty()) {
		psoKey = meshName + "_" + mName + "_PSO";
	} else {
		// GetFullName()を使用してメッシュ名が設定されている場合はそれを使用
		psoKey = GetFullName() + "_PSO";
	}

	Console::Print(
		std::format("PSOキーを生成: {}\n", psoKey),
		kConTextColorCompleted,
		Channel::ResourceSystem
	);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;

	// ルートシグネチャの設定
	auto rootSig = GetOrCreateRootSignature(device);
	if (!rootSig) {
		Console::Print("ルートシグネチャの取得に失敗しました\n", kConTextColorError,
		               Channel::ResourceSystem);
		return nullptr;
	}
	desc.pRootSignature = rootSig;

	// シェーダーの設定
	if (mShader->GetVertexShaderBlob()) {
		desc.VS = {
			mShader->GetVertexShaderBlob()->GetBufferPointer(),
			mShader->GetVertexShaderBlob()->GetBufferSize()
		};
	}
	if (mShader->GetPixelShaderBlob()) {
		desc.PS = {
			mShader->GetPixelShaderBlob()->GetBufferPointer(),
			mShader->GetPixelShaderBlob()->GetBufferSize()
		};
	}

	// パイプラインステートの取得または作成
	mPipelineState = PipelineManager::GetOrCreatePipelineState(
		device, psoKey, desc);
	return mPipelineState.Get();
}

ID3D12RootSignature* Material::GetOrCreateRootSignature(
	[[maybe_unused]] ID3D12Device* device) {
	if (!mRootSignature) {
		// ルートシグネチャがない場合は作成
		std::string       key  = mShader->GetName();
		RootSignatureDesc desc = {};

		// シェーダーリソースマップからリソースを解析
		const auto& resourceMap = mShader->GetResourceRegisterMap();
		std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;

		// リソースの分類とルートパラメータの追加
		for (const auto& [name, resourceInfo] : resourceMap) {
			// ShaderVisibilityを文字列に変換
			std::string visibilityStr;
			switch (resourceInfo.visibility) {
			case D3D12_SHADER_VISIBILITY_VERTEX:
				visibilityStr = "VS";
				break;
			case D3D12_SHADER_VISIBILITY_PIXEL:
				visibilityStr = "PS";
				break;
			case D3D12_SHADER_VISIBILITY_ALL:
				visibilityStr = "ALL";
				break;
			case D3D12_SHADER_VISIBILITY_GEOMETRY:
				visibilityStr = "GS";
				break;
			// 他のシェーダーステージがあれば追加
			default:
				visibilityStr = "UNKNOWN";
				break;
			}

			if (resourceInfo.type == D3D_SIT_CBUFFER) {
				D3D12_ROOT_PARAMETER param      = {};
				param.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
				param.ShaderVisibility          = resourceInfo.visibility;
				param.Descriptor.ShaderRegister = resourceInfo.bindPoint;
				param.Descriptor.RegisterSpace  = 0;
				desc.parameters.emplace_back(param);

				Console::Print(
					std::format(
						"定数バッファを追加: {} (register b{}, visibility: {})\n",
						name,
						resourceInfo.bindPoint,
						visibilityStr
					),
					kConTextColorCompleted,
					Channel::ResourceSystem
				);
			} else if (resourceInfo.type == D3D_SIT_TEXTURE) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = resourceInfo.bindPoint;
				range.RegisterSpace = 0;
				range.OffsetInDescriptorsFromTableStart =
					D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				srvRanges.emplace_back(range);

				Console::Print(
					std::format(
						"テクスチャを追加: {} (register t{}, visibility: {})\n",
						name,
						range.BaseShaderRegister,
						visibilityStr
					),
					kConTextColorCompleted,
					Channel::ResourceSystem
				);
			}
		}

		// テクスチャがある場合はディスクリプタテーブルを追加
		if (!srvRanges.empty()) {
			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(
				srvRanges.size());
			param.DescriptorTable.pDescriptorRanges = srvRanges.data();
			desc.parameters.emplace_back(param);

		// サンプラーの追加
		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor =
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD           = 0.0f;
		samplerDesc.MaxLOD           = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister   = 0;
		samplerDesc.RegisterSpace    = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		desc.samplers.emplace_back(samplerDesc);
		}

		// INPUT_LAYOUTフラグを設定
		desc.flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		mRootSignature =
			RootSignatureManager2::GetOrCreateRootSignature(key, desc)->Get();
	}
	return mRootSignature.Get();
}

void Material::InitializeRootSignature() const {
	if (!mShader) {
		Console::Print("シェーダーが設定されていません\n", kConTextColorError,
		               Channel::ResourceSystem);
	}
}

void Material::Shutdown() {
	mRootSignature.Reset();
	mPipelineState.Reset();
}

const std::string& Material::GetName() const { return mName; }

const std::unordered_map<std::string, std::string>&
Material::GetTextures() const {
	return mTextures;
}

void Material::SetMeshName(const std::string& meshName) {
	mEshName = meshName;
}

std::string Material::GetMeshName() const {
	return mEshName;
}

std::string Material::GetFullName() const {
	if (!mEshName.empty()) {
		return mEshName + "_" + mName;
	}
	return mName;
}

std::string Material::GenerateBufferKey(D3D12_SHADER_VISIBILITY visibility,
                                        UINT                    bindPoint) {
	std::string key;
	switch (visibility) {
	case D3D12_SHADER_VISIBILITY_ALL:
		key = "ALL";
		break;
	case D3D12_SHADER_VISIBILITY_VERTEX:
		key = "VS";
		break;
	case D3D12_SHADER_VISIBILITY_HULL:
		key = "HS";
		break;
	case D3D12_SHADER_VISIBILITY_DOMAIN:
		key = "DS";
		break;
	case D3D12_SHADER_VISIBILITY_GEOMETRY:
		key = "GS";
		break;
	case D3D12_SHADER_VISIBILITY_PIXEL:
		key = "PS";
		break;
	case D3D12_SHADER_VISIBILITY_AMPLIFICATION:
		key = "AS";
		break;
	case D3D12_SHADER_VISIBILITY_MESH:
		key = "MS";
		break;
	default:
		key = "UNKNOWN";
		break;
	}
	return key + "_" + std::to_string(bindPoint);
}
