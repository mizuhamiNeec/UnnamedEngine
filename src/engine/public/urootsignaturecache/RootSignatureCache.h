#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <vector>

#include <wrl/client.h>

namespace Unnamed {
	class GraphicsDevice;

	enum class RangeType {
		SRV,
		UAV,
		CBV,
		SAMPLER
	};

	struct RangeDesc {
		RangeType type;
		UINT      baseRegister;
		UINT      count;
		UINT      space = 0;
	};

	struct RootParamDesc {
		enum class Kind { TABLE, ROOT_CBV, ROOT32_BIT_CONST };

		Kind                   kind = Kind::TABLE;
		std::vector<RangeDesc> ranges; // TABLEの時に使用

		// RootCBV用
		UINT cbvRegister = 0;
		UINT cbvSpace    = 0;

		// 32BitConst用
		UINT num32Bit      = 0;
		UINT constRegister = 0;
		UINT constSpace    = 0;
	};

	struct StaticSamplerDesc {
		D3D12_SAMPLER_DESC desc;
		UINT               reg;
		UINT               space = 0;
	};

	struct RootSignatureDesc {
		std::vector<RootParamDesc>     params;
		std::vector<StaticSamplerDesc> staticSamplers;
		D3D12_ROOT_SIGNATURE_FLAGS     flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	};

	struct RootSignatureHandle {
		uint32_t id = UINT32_MAX;
	};

	class RootSignatureCache {
	public:
		explicit RootSignatureCache(GraphicsDevice* graphicsDevice);
		RootSignatureHandle GetOrCreate(const RootSignatureDesc& desc);
		ID3D12RootSignature* Get(RootSignatureHandle handle) const;
		D3D12_ROOT_SIGNATURE_DESC& GetDesc(RootSignatureHandle rootSignature);

	private:
		static size_t Hash(const RootSignatureDesc& desc);

	private:
		GraphicsDevice* mGraphicsDevice = nullptr;

		struct Entry {
			size_t                                      hash;
			Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
			D3D12_ROOT_SIGNATURE_DESC                   rootSignatureDesc;
			std::vector<Entry>                          entries;
			std::unordered_map<size_t, uint32_t>        hashToIndex;
		};

		std::vector<Entry>                   mEntries;
		std::unordered_map<size_t, uint32_t> mHashToIndex;
	};
}
