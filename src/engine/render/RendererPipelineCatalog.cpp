#include "RendererPipelineCatalog.h"

namespace Unnamed::Render {
	GraphicsPipelineSpec RendererPipelineCatalog::MakeFullscreenPreset(
		std::string          debugName,
		const AssetID        shaderProgramId,
		ID3D12RootSignature* rootSignature,
		const DXGI_FORMAT    rtvFormat
	) {
		GraphicsPipelineSpec spec         = {};
		spec.debugName                    = std::move(debugName);
		spec.shaderProgramId              = shaderProgramId;
		spec.rootSignature                = rootSignature;
		spec.psoTemplate.rootSignature    = rootSignature;
		spec.psoTemplate.vertexLayout     = std::nullopt;
		spec.psoTemplate.rtvFormat        = rtvFormat;
		spec.psoTemplate.depthEnable      = false;
		spec.psoTemplate.depthWriteEnable = false;
		spec.psoTemplate.dsvFormat        = DXGI_FORMAT_UNKNOWN;
		spec.psoTemplate.depthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
		return spec;
	}

	GraphicsPipelineSpec RendererPipelineCatalog::MakeGeometryPreset(
		std::string                  debugName,
		const AssetID                shaderProgramId,
		ID3D12RootSignature*         rootSignature,
		const DXGI_FORMAT            rtvFormat,
		const DXGI_FORMAT            dsvFormat,
		const Rhi::VertexLayoutDesc& vertexLayout
	) {
		GraphicsPipelineSpec spec         = {};
		spec.debugName                    = std::move(debugName);
		spec.shaderProgramId              = shaderProgramId;
		spec.rootSignature                = rootSignature;
		spec.psoTemplate.rootSignature    = rootSignature;
		spec.psoTemplate.vertexLayout     = vertexLayout;
		spec.psoTemplate.rtvFormat        = rtvFormat;
		spec.psoTemplate.depthEnable      = true;
		spec.psoTemplate.depthWriteEnable = true;
		spec.psoTemplate.dsvFormat        = dsvFormat;
		spec.psoTemplate.depthFunc        = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		return spec;
	}

	GraphicsPipelineSpec RendererPipelineCatalog::MakeSpritePreset(
		std::string                  debugName,
		const AssetID                shaderProgramId,
		ID3D12RootSignature*         rootSignature,
		const DXGI_FORMAT            rtvFormat,
		const Rhi::VertexLayoutDesc& vertexLayout
	) {
		GraphicsPipelineSpec spec         = {};
		spec.debugName                    = std::move(debugName);
		spec.shaderProgramId              = shaderProgramId;
		spec.rootSignature                = rootSignature;
		spec.psoTemplate.rootSignature    = rootSignature;
		spec.psoTemplate.vertexLayout     = vertexLayout;
		spec.psoTemplate.rtvFormat        = rtvFormat;
		spec.psoTemplate.depthEnable      = false;
		spec.psoTemplate.depthWriteEnable = false;
		spec.psoTemplate.dsvFormat        = DXGI_FORMAT_UNKNOWN;
		spec.psoTemplate.depthFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
		spec.psoTemplate.cullMode         = D3D12_CULL_MODE_NONE;
		spec.psoTemplate.blendEnable      = true;
		spec.psoTemplate.srcBlend         = D3D12_BLEND_SRC_ALPHA;
		spec.psoTemplate.destBlend        = D3D12_BLEND_INV_SRC_ALPHA;
		spec.psoTemplate.srcBlendAlpha    = D3D12_BLEND_ONE;
		spec.psoTemplate.destBlendAlpha   = D3D12_BLEND_INV_SRC_ALPHA;
		return spec;
	}

	GraphicsPipelineSpec RendererPipelineCatalog::MakeLinePreset(
		std::string                  debugName,
		const AssetID                shaderProgramId,
		ID3D12RootSignature*         rootSignature,
		const DXGI_FORMAT            rtvFormat,
		const DXGI_FORMAT            dsvFormat,
		const Rhi::VertexLayoutDesc& vertexLayout
	) {
		GraphicsPipelineSpec spec = {};
		spec.debugName = std::move(debugName);
		spec.shaderProgramId = shaderProgramId;
		spec.rootSignature = rootSignature;
		spec.psoTemplate.rootSignature = rootSignature;
		spec.psoTemplate.vertexLayout = vertexLayout;
		spec.psoTemplate.rtvFormat = rtvFormat;
		spec.psoTemplate.depthEnable = true;
		spec.psoTemplate.depthWriteEnable = true;
		spec.psoTemplate.dsvFormat = dsvFormat;
		spec.psoTemplate.depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		spec.psoTemplate.cullMode = D3D12_CULL_MODE_NONE;
		spec.psoTemplate.primitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		return spec;
	}

	ComputePipelineSpec RendererPipelineCatalog::MakeComputePreset(
		std::string          debugName,
		const AssetID        shaderProgramId,
		ID3D12RootSignature* rootSignature
	) {
		ComputePipelineSpec spec       = {};
		spec.debugName                 = std::move(debugName);
		spec.shaderProgramId           = shaderProgramId;
		spec.rootSignature             = rootSignature;
		spec.psoTemplate.rootSignature = rootSignature;
		return spec;
	}
}
