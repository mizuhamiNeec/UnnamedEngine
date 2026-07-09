#include "PipelineRegistry.h"

#include <string_view>

#include "../RenderDevice.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	namespace {
		constexpr std::string_view kChannel = "PipelineReg";

		bool ResolveShaderProgramStageKey(
			const RenderDevice&    renderDevice,
			const AssetID          shaderProgramId,
			const std::string_view stage,
			ShaderKey&             outKey
		) {
			const auto& assetManager = renderDevice.GetAssetManager();
			const auto* program      = assetManager.Get<ShaderProgramAssetData>(
				shaderProgramId
			);
			if (!program) {
				return false;
			}

			const std::optional<ShaderProgramStage>* src = nullptr;
			if (stage == "vs") {
				src = &program->vs;
			}
			if (stage == "ps") {
				src = &program->ps;
			}
			if (stage == "cs") {
				src = &program->cs;
			}
			if (!src || !src->has_value()) {
				return false;
			}

			const AssetID shaderSourceId = src->value().shaderSourceAssetId;
			if (shaderSourceId == kInvalidAssetID) {
				return false;
			}

			outKey.shaderSourceId = shaderSourceId;
			outKey.entry          = src->value().entry;
			outKey.profile        = src->value().profile;
			outKey.defines        = src->value().defines;
			return true;
		}
	}

	PipelineHandle PipelineRegistry::RegisterGraphics(
		const GraphicsPipelineSpec& spec
	) {
		GraphicsEntry entry = {};
		entry.spec          = spec;
		mGraphics.emplace_back(std::move(entry));
		return PipelineHandle{
			.kind  = PIPELINE_KIND::GRAPHICS,
			.index = static_cast<uint32_t>(mGraphics.size() - 1)
		};
	}

	PipelineHandle PipelineRegistry::RegisterCompute(
		const ComputePipelineSpec& spec
	) {
		ComputeEntry entry = {};
		entry.spec         = spec;
		mCompute.emplace_back(std::move(entry));
		return PipelineHandle{
			.kind  = PIPELINE_KIND::COMPUTE,
			.index = static_cast<uint32_t>(mCompute.size() - 1)
		};
	}

	void PipelineRegistry::Clear() {
		mGraphics.clear();
		mCompute.clear();
	}

	PipelineResolveResult PipelineRegistry::ResolveAll(
		RenderDevice& renderDevice, const PIPELINE_RESOLVE_SCOPE scope
	) {
		PipelineResolveResult result = {};
		for (auto& entry : mGraphics) {
			if (
				scope == PIPELINE_RESOLVE_SCOPE::REQUIRED_ONLY &&
				entry.spec.startupRequirement !=
				PIPELINE_STARTUP_REQUIREMENT::REQUIRED
			) {
				continue;
			}
			++result.requestedCount;
			entry.resolved = {};
			if (
				entry.spec.shaderProgramId == kInvalidAssetID ||
				entry.spec.rootSignature == nullptr
			) {
				if (!entry.resolveFailureLogged) {
					Error(
						kChannel,
						"Invalid graphics pipeline specification '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			GraphicsPsoKey key = entry.spec.psoTemplate;
			key.rootSignature  = entry.spec.rootSignature;

			if (!ResolveShaderProgramStageKey(
				renderDevice, entry.spec.shaderProgramId, "vs", key.vs
			)) {
				if (!entry.resolveFailureLogged) {
					Warning(
						kChannel,
						"Failed to resolve VS stage for graphics pipeline '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}
			if (!ResolveShaderProgramStageKey(
				renderDevice, entry.spec.shaderProgramId, "ps", key.ps
			)) {
				if (!entry.resolveFailureLogged) {
					Warning(
						kChannel,
						"Failed to resolve PS stage for graphics pipeline '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			ID3D12PipelineState* pso =
				renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(key);
			if (!pso) {
				if (!entry.resolveFailureLogged) {
					Error(
						kChannel,
						"Failed to create graphics PSO for pipeline '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			entry.resolved.rootSignature = entry.spec.rootSignature;
			entry.resolved.pso           = pso;
			entry.resolveFailureLogged   = false;
			++result.resolvedCount;
		}

		for (auto& entry : mCompute) {
			if (
				scope == PIPELINE_RESOLVE_SCOPE::REQUIRED_ONLY &&
				entry.spec.startupRequirement !=
				PIPELINE_STARTUP_REQUIREMENT::REQUIRED
			) {
				continue;
			}
			++result.requestedCount;
			entry.resolved = {};
			if (
				entry.spec.shaderProgramId == kInvalidAssetID ||
				entry.spec.rootSignature == nullptr
			) {
				if (!entry.resolveFailureLogged) {
					Error(
						kChannel,
						"Invalid compute pipeline specification '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			ComputePipelineKey key = entry.spec.psoTemplate;
			key.rootSignature      = entry.spec.rootSignature;
			if (!ResolveShaderProgramStageKey(
				renderDevice, entry.spec.shaderProgramId, "cs", key.cs
			)) {
				if (!entry.resolveFailureLogged) {
					Warning(
						kChannel,
						"Failed to resolve CS stage for compute pipeline '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			ID3D12PipelineState* pso =
				renderDevice.GetPipelineCache().GetOrCreateComputePso(key);
			if (!pso) {
				if (!entry.resolveFailureLogged) {
					Error(
						kChannel,
						"Failed to create compute PSO for pipeline '{}'",
						entry.spec.debugName
					);
					entry.resolveFailureLogged = true;
					++result.newlyFailedCount;
				}
				continue;
			}

			entry.resolved.rootSignature = entry.spec.rootSignature;
			entry.resolved.pso           = pso;
			entry.resolveFailureLogged   = false;
			++result.resolvedCount;
		}
		return result;
	}

	const ResolvedGraphicsPipeline* PipelineRegistry::GetGraphics(
		const PipelineHandle handle
	) const {
		if (!handle.IsValid() || handle.kind != PIPELINE_KIND::GRAPHICS) {
			return nullptr;
		}
		if (handle.index >= mGraphics.size()) {
			return nullptr;
		}
		return &mGraphics[handle.index].resolved;
	}

	const ResolvedComputePipeline* PipelineRegistry::GetCompute(
		const PipelineHandle handle
	) const {
		if (!handle.IsValid() || handle.kind != PIPELINE_KIND::COMPUTE) {
			return nullptr;
		}
		if (handle.index >= mCompute.size()) {
			return nullptr;
		}
		return &mCompute[handle.index].resolved;
	}
}
