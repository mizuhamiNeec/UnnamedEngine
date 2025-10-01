#include <pch.h>

//-----------------------------------------------------------------------------

#include "UMaterialRuntime.h"

#include <filesystem>

#include <engine/uprogrambuilder/UProgramBuilder.h>
#include <engine/urenderer/GraphicsDevice.h>

#include <runtime/assets/core/UAssetManager.h>
#include <runtime/render/resources/ShaderLibrary.h>

#include "core/UnnamedMacro.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "UMaterialRuntime";

	static D3D12_SAMPLER_DESC LinerWrap() {
		D3D12_SAMPLER_DESC s = {};
		s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		s.MinLOD = 0.0f;
		s.MaxLOD = D3D12_FLOAT32_MAX;
		s.MipLODBias = 0.0f;
		s.MaxAnisotropy = 1;
		s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		return s;
	}

	struct MaterialCBData {
		Vec4     BaseColor; // 16
		float    Metallic;  // 4
		float    ForceMipNorm;
		uint32_t UseForcedMip;
		float    _pad0;
		Vec4     Extra[14];
	};

	bool UMaterialRuntime::BuildCPU(
		UAssetManager*      assetManager,
		ShaderLibrary*      shaderLibrary,
		RootSignatureCache* rootSignatureCache,
		UPipelineCache*     pipelineCache,
		GraphicsDevice*     graphicsDevice
	) {
		if (
			!assetManager || !shaderLibrary || !rootSignatureCache ||
			!pipelineCache || !graphicsDevice
		) {
			return false;
		}
		if (materialAsset == kInvalidAssetID) {
			return false;
		}

		mGraphicsDevice = graphicsDevice;

		const auto* m = assetManager->Get<MaterialAssetData>(materialAsset);
		if (!m) {
			return false;
		}

		mNameToSlot.clear();
		mExtraCache.fill(Vec4::zero);

		// マテリアル定数をCPUにキャッシュ
		if (
			auto it = m->float4Params.find("BaseColor");
			it != m->float4Params.end()
		) {
			const auto& v = it->second;
			mBaseColor    = Vec4{v[0], v[1], v[2], v[3]};
		} else {
			mBaseColor = Vec4::one;
		}

		if (
			auto it = m->floatParams.find("Metallic");
			it != m->floatParams.end()
		) {
			mMetallic = it->second;
		} else {
			mMetallic = 0.0f;
		}

		std::vector<std::pair<std::string, MaterialParam>> list;
		for (auto& [name, p] : m->params) {
			// 使っているパラメータなので無視
			if (name == "BaseColor" || name == "Metallic") {
				continue;
			}
			list.emplace_back(name, p);
		}

		std::ranges::sort(list, [](const auto& a, const auto& b) {
			return a.first < b.first;
		});

		int slot = 0;
		for (auto& [name, p] : list) {
			if (slot >= 14) {
				Warning(kChannel, "Material has too many parameters");
			}
			mNameToSlot[name] = slot;
			Vec4 v;
			if (p.type == ParamType::FLOAT) {
				v = {p.v[0], 0, 0, 0};
			} else {
				v = {p.v[0], p.v[1], p.v[2], p.v[3]};
			}
			mExtraCache[slot] = v;
			slot++;
		}

		// シェーダーの取得
		const ShaderBlob* vs      = nullptr;
		const ShaderBlob* ps      = nullptr;
		auto              defines = m->defines;

		if (m->programBody != kInvalidAssetID) {
			ProgramBuildInput pbi;
			pbi.abiInclude = "./content/core/materials/src/MaterialABI.hlsli";
			pbi.bodyPath   = assetManager->Meta(m->programBody).sourcePath;
			pbi.metaPath   = assetManager->Meta(m->programMeta).sourcePath;

			GeneratedProgram gen;
			std::string      err;
			if (!UProgramBuilder::BuildFromBody(pbi, gen, &err)) {
				DevMsg(kChannel, "Program build failed: {}", err);
				return false;
			}

			ShaderVariantKey vKeyVS = {
				.asset = kInvalidAssetID, .defines = {},
				.entryPoint = gen.vsEntry,
				.target = "vs_6_0"
			};
			ShaderVariantKey vKeyPS = {
				.asset = kInvalidAssetID, .defines = {},
				.entryPoint = gen.psEntry,
				.target = "ps_6_0"
			};

			vs = shaderLibrary->GetOrCompileFromString(
				gen.hlsl, vKeyVS, ("#VS" + pbi.bodyPath).c_str()
			);
			ps = shaderLibrary->GetOrCompileFromString(
				gen.hlsl, vKeyPS, ("#PS" + pbi.bodyPath).c_str()
			);
		} else {
			if (m->shader != kInvalidAssetID) {
				vs = shaderLibrary->GetOrCompile(
					{m->shader, defines, m->entryVS, "vs_6_0"}
				);
				ps = shaderLibrary->GetOrCompile(
					{m->shader, defines, m->entryPS, "ps_6_0"}
				);
			} else {
				if (m->shaderVS != kInvalidAssetID) {
					vs = shaderLibrary->GetOrCompile(
						{m->shaderVS, defines, m->entryVS, "vs_6_0"}
					);
				}
				if (m->shaderPS != kInvalidAssetID) {
					ps = shaderLibrary->GetOrCompile(
						{m->shaderPS, defines, m->entryPS, "ps_6_0"}
					);
				}
			}
		}

		if (!vs || !ps) {
			return false;
		}

		// ルートシグネチャ
		RootSignatureDesc rs   = {};
		RootParamDesc     pSRV = {};
		pSRV.kind              = RootParamDesc::Kind::TABLE;
		pSRV.ranges.emplace_back(RangeType::SRV, 0, 1, 0); // t0 1個
		rs.params.emplace_back(pSRV);

		RootParamDesc pCBV = {};
		pCBV.kind          = RootParamDesc::Kind::ROOT_CBV;
		pCBV.cbvRegister   = 0; // b0
		pCBV.cbvSpace      = 0;
		rs.params.emplace_back(pCBV);

		RootParamDesc pFrameCB = {};
		pFrameCB.kind          = RootParamDesc::Kind::ROOT_CBV;
		pFrameCB.cbvRegister   = 1; // b1
		pFrameCB.cbvSpace      = 0;
		rs.params.emplace_back(pFrameCB);

		RootParamDesc pObjectCB = {};
		pObjectCB.kind          = RootParamDesc::Kind::ROOT_CBV;
		pObjectCB.cbvRegister   = 2; // b2
		pObjectCB.cbvSpace      = 0;
		rs.params.emplace_back(pObjectCB);

		StaticSamplerDesc s0 = {};
		s0.desc              = LinerWrap();
		s0.reg               = 0; // s0
		s0.space             = 0;
		rs.staticSamplers.emplace_back(s0);

		for (auto param : rs.params) {
			Msg(
				kChannel,
				"Params: Kind({}), {}",
				static_cast<int>(param.kind),
				param.cbvRegister
			);
		}

		root = rootSignatureCache->GetOrCreate(rs);
		if (root.id == UINT32_MAX) {
			return false;
		}

		rootParams.srvTable   = 0;
		rootParams.materialCB = 1;
		rootParams.frameCB    = 2;
		rootParams.objectCB   = 3;

		// PSO
		PipelineDesc pd  = {};
		pd.rootSignature = root;
		pd.inputLayout   = {
			{
				"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
				D3D12_APPEND_ALIGNED_ELEMENT
			},
			{
				"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT,
				D3D12_APPEND_ALIGNED_ELEMENT
			},
			{
				"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
				D3D12_APPEND_ALIGNED_ELEMENT
			}
		};

		pd.depth                = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pd.depth.DepthEnable    = TRUE;
		pd.depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pd.depth.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		pd.dsv = DXGI_FORMAT_D32_FLOAT;

		pd.rasterizer.CullMode = D3D12_CULL_MODE_BACK;

		pd.vs = vs;
		pd.ps = ps;
		pso   = pipelineCache->GetOrCreate(pd);
		if (pso.id == UINT32_MAX) {
			return false;
		}

		// テクスチャスロットをCPUに保持
		mTextureSlots.clear();
		for (auto& [s, assetID] : m->textureSlots) {
			TextureSlot ts;
			ts.name   = s;
			ts.asset  = assetID;
			ts.handle = {};
			mTextureSlots.emplace_back(std::move(ts));
		}

		// 固定CB
		if (!mCB) {
			const UINT64 size = static_cast<UINT64>(mCBStride) *
				mFramesInFlight;
			D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_UPLOAD};
			D3D12_RESOURCE_DESC   rd = {};
			rd.Dimension             = D3D12_RESOURCE_DIMENSION_BUFFER;
			rd.Width                 = size;
			rd.Height                = 1;
			rd.DepthOrArraySize      = 1;
			rd.MipLevels             = 1;
			rd.Format                = DXGI_FORMAT_UNKNOWN;
			rd.SampleDesc            = {.Count = 1, .Quality = 0};
			rd.Layout                = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			auto hr = mGraphicsDevice->Device()->CreateCommittedResource(
				&hp, D3D12_HEAP_FLAG_NONE, &rd,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
				IID_PPV_ARGS(&mCB)
			);
			if (FAILED(hr)) {
				return false;
			}

			D3D12_RANGE noRead = {0, 0};
			hr = mCB->Map(0, &noRead, reinterpret_cast<void**>(&mCBMapped));
			if (FAILED(hr)) {
				return false;
			}
		}

		mMainTexMipCount = 1;
		for (auto& s : mTextureSlots) {
			if (s.name == "MainTex" && s.asset != kInvalidAssetID) {
				if (auto* ta = assetManager->Get<
					TextureAssetData>(s.asset)) {
					mMainTexMipCount = ta->mips.empty() ?
						                   1u :
						                   static_cast<uint32_t>(ta->mips.
							                   size());
				}
				break;
			}
		}

		// ファイル監視
		{
			ClearWatch();
			if (materialAsset != kInvalidAssetID) {
				AddWatch(assetManager, materialAsset, WatchType::MAT);
			}

			if (m->programBody != kInvalidAssetID) {
				AddWatch(assetManager, m->programBody, WatchType::PROGRAM_BODY);
			}
			if (m->programMeta != kInvalidAssetID) {
				AddWatch(assetManager, m->programMeta, WatchType::PROGRAM_META);
			}
			for (auto& ts : mTextureSlots) {
				if (ts.asset != kInvalidAssetID) {
					AddWatch(assetManager, ts.asset, WatchType::TEXTURE);
				}
			}
		}

		mGPUReady = false;
		return true;
	}

	bool UMaterialRuntime::RealizeGPU(
		RenderResourceManager*     renderResourceManager,
		ID3D12GraphicsCommandList* commandList
	) {
		if (!renderResourceManager || !commandList) {
			return false;
		}

		// スロットの実体化
		for (auto& slot : mTextureSlots) {
			if (!slot.handle.IsValid() && slot.asset != kInvalidAssetID) {
				slot.handle = renderResourceManager->AcquireTexture(
					slot.asset, commandList);
				// 失敗しても近いうちに読み込まれます。裁判も起こします。裁判所にも問答無用で来てもらいます。
			}
		}

		bool ok = true;
		for (auto& slot : mTextureSlots) {
			if (slot.name == "MainTex") {
				ok = slot.handle.IsValid();
				break;
			}
		}
		mGPUReady = ok;
		return ok;
	}

	void UMaterialRuntime::InvalidateGPU(
		RenderResourceManager* renderResourceManager,
		ID3D12Fence*           fence,
		const uint64_t         value
	) {
		if (!renderResourceManager) {
			return;
		}
		for (auto& slot : mTextureSlots) {
			if (slot.handle.IsValid()) {
				renderResourceManager->ReleaseTexture(
					slot.handle, fence, value);
				slot.handle = {};
			}
		}
		mGPUReady = false;
	}

	uint32_t UMaterialRuntime::DetectChanges() {
		uint32_t mask = RELOAD_NONE;
		for (auto& w : mWatchedFiles) {
			if (w.id == kInvalidAssetID || w.path.empty()) {
				continue;
			}
			uint64_t    size  = 0;
			std::time_t mtime = 0;

			// き、消えた!!?
			if (!StartFile(w.path, size, mtime)) {
				// まあ別に消える分には...
				continue;
			}

			// おいみろよ!
			if (size != w.size || mtime != w.mtime) {
				DevMsg(
					kChannel,
					"HotReload: {} changed ({})\n",
					w.path,
					w.type == WatchType::TEXTURE ?
						"TEXTURE" :
						"Shader/Mat"
				);

				switch (w.type) {
				case WatchType::MAT:
					mask |= RELOAD_CPU | RELOAD_TEXTURE;
					break;
				case WatchType::PROGRAM_BODY:
				case WatchType::PROGRAM_META:
					mask |= RELOAD_CPU;
					break;

				case WatchType::TEXTURE:
					mask |= RELOAD_TEXTURE;
					break;
				default: ;
				}

				w.size  = size;
				w.mtime = mtime;
			}
		}
		return mask;
	}

	bool UMaterialRuntime::IsGPUReady() const { return mGPUReady; }

	void UMaterialRuntime::Apply(
		ID3D12GraphicsCommandList*   commandList,
		const RenderResourceManager* renderResourceManager,
		const uint32_t               backIndex,
		float                        timeSec
	) const {
		for (auto& slot : mTextureSlots) {
			if (slot.name == "MainTex" && slot.handle.IsValid()) {
				commandList->SetGraphicsRootDescriptorTable(
					0, renderResourceManager->GetSrvGPU(slot.handle));
				break;
			}
		}

		// CBV(b0) 固定CBに書き込んでオフセットを指定
		MaterialCBData cb = {};
		cb.BaseColor      = mBaseColor;
		cb.Metallic       = mMetallic;

		cb.UseForcedMip = dbgForceMip ? 1u : 0u;
		cb.ForceMipNorm = 0.0f;

		if (dbgForceMip) {
			int mipIdx;
			if (dbgOscillate) {
				const float phase = 0.5f + 0.5f * sin(
					timeSec * Math::pi * 2.0f * dbgSpeedHz);
				cb.ForceMipNorm = phase;
			} else if (dbgFixedMip >= 0 && mMainTexMipCount > 1) {
				cb.ForceMipNorm = static_cast<float>(dbgFixedMip) /
					static_cast<float>(mMainTexMipCount - 1);
			}

			if (mMainTexMipCount > 1) {
				mipIdx = std::clamp(
					static_cast<int>(
						std::lround(
							static_cast<int>(cb.ForceMipNorm) * (
								mMainTexMipCount - 1))
					),
					0,
					static_cast<int>(mMainTexMipCount) - 1
				);
			} else {
				mipIdx = 0;
			}

			if (mipIdx != mPrevLoggedMip) {
				mPrevLoggedMip = mipIdx;
				DevMsg(
					kChannel,
					"Forced MIP = {} / {} (norm={:.3f})",
					mipIdx, mMainTexMipCount, cb.ForceMipNorm
				);
			}
		}

		for (int i = 0; i < 14; ++i) {
			cb.Extra[i] = mExtraCache[i];
		}

		const size_t offset = static_cast<size_t>(mCBStride) * (backIndex %
			mFramesInFlight);
		memcpy(mCBMapped + offset, &cb, sizeof(cb));

		const auto gpuVA = mCB->GetGPUVirtualAddress() + offset;
		commandList->SetGraphicsRootConstantBufferView(1, gpuVA);
	}

	void UMaterialRuntime::Release(
		RenderResourceManager* renderResourceManager,
		ID3D12Fence*           fence,
		const uint64_t         value
	) {
		InvalidateGPU(renderResourceManager, fence, value);

		if (mCBMapped) {
			mCB->Unmap(0, nullptr);
			mCBMapped = nullptr;
		}
		mCB.Reset();
		mTextureSlots.clear();
		mGPUReady       = false;
		root            = {};
		pso             = {};
		mGraphicsDevice = nullptr;
	}

	bool UMaterialRuntime::StartFile(
		const std::string& path, uint64_t& size, std::time_t& mtime
	) {
		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) {
			return false;
		}
		size    = std::filesystem::file_size(path);
		auto ft = std::filesystem::last_write_time(path, ec);
		if (ec) {
			return false;
		}

		const auto sctp = std::chrono::time_point_cast<
			std::chrono::system_clock::duration>(
			ft - decltype(ft)::clock::now() + std::chrono::system_clock::now()
		);
		mtime = std::chrono::system_clock::to_time_t(sctp);
		return true;
	}

	void UMaterialRuntime::ClearWatch() {
		mWatchedFiles.clear();
	}

	void UMaterialRuntime::AddWatch(
		const UAssetManager* assetManager, const AssetID id,
		const WatchType      type
	) {
		if (id == kInvalidAssetID) {
			return;
		}

		std::string p     = assetManager->Meta(id).sourcePath;
		uint64_t    size  = 0;
		std::time_t mtime = 0;
		StartFile(p, size, mtime);
		mWatchedFiles.emplace_back(WatchedFile{id, p, size, mtime, type});
	}
}
