#include "ShaderCompileUnit.h"

#include <algorithm>
#include <unordered_set>

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderSourceAssetData.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	ShaderCompileUnitBuilder::ShaderCompileUnitBuilder(
		const AssetManager& assetManager
	) : mAssetManager(assetManager) {
	}

	std::optional<ShaderCompileUnit> ShaderCompileUnitBuilder::Build(
		const AssetID rootShaderSourceId
	) const {
		const auto* rootSource = mAssetManager.Get<ShaderSourceAssetData>(
			rootShaderSourceId
		);
		if (!rootSource) {
			Error(
				"ShaderCompileUnit",
				"Root ShaderSource payload is unavailable: assetId={}",
				rootShaderSourceId
			);
			return std::nullopt;
		}

		std::vector<AssetID> pending = {rootShaderSourceId};
		std::unordered_set<AssetID> visited;
		std::vector<AssetID> includeAssetIds;
		while (!pending.empty()) {
			const AssetID sourceAssetId = pending.back();
			pending.pop_back();
			if (!visited.emplace(sourceAssetId).second) {
				continue;
			}

			const auto* source = mAssetManager.Get<ShaderSourceAssetData>(
				sourceAssetId
			);
			if (!source) {
				Error(
					"ShaderCompileUnit",
					"Transitive ShaderSource payload is unavailable: rootAssetId={} dependencyAssetId={}",
					rootShaderSourceId,
					sourceAssetId
				);
				return std::nullopt;
			}
			if (sourceAssetId != rootShaderSourceId) {
				includeAssetIds.emplace_back(sourceAssetId);
			}
			for (const ResolvedShaderInclude& include : source->resolvedIncludes) {
				if (include.shaderSourceAssetId == kInvalidAssetID) {
					return std::nullopt;
				}
				pending.emplace_back(include.shaderSourceAssetId);
			}
		}

		std::ranges::sort(
			includeAssetIds,
			[this](const AssetID lhs, const AssetID rhs) {
				const std::string lhsPath = BuildDiagnosticPath(lhs);
				const std::string rhsPath = BuildDiagnosticPath(rhs);
				if (lhsPath != rhsPath) {
					return lhsPath < rhsPath;
				}
				return lhs < rhs;
			}
		);

		std::unordered_map<AssetID, std::string> internalNames;
		for (size_t index = 0; index < includeAssetIds.size(); ++index) {
			internalNames.emplace(
				includeAssetIds[index],
				"__unnamed_shader_include__/" + std::to_string(index)
			);
		}

		ShaderCompileUnit unit = {};
		unit.rootDiagnosticPath = BuildDiagnosticPath(rootShaderSourceId);
		unit.rootMountId        = rootSource->mountId;
		const std::optional<std::string> rewrittenRoot = RewriteSource(
			rootShaderSourceId, internalNames
		);
		if (!rewrittenRoot.has_value()) {
			return std::nullopt;
		}
		unit.rewrittenRootSource = *rewrittenRoot;
		unit.includeEntries.reserve(includeAssetIds.size());

		for (const AssetID includeAssetId : includeAssetIds) {
			const auto* source = mAssetManager.Get<ShaderSourceAssetData>(
				includeAssetId
			);
			const std::optional<std::string> rewritten = RewriteSource(
				includeAssetId, internalNames
			);
			if (!source || !rewritten.has_value()) {
				return std::nullopt;
			}

			ShaderCompileIncludeEntry entry = {
				.internalName    = internalNames.at(includeAssetId),
				.virtualPath     = source->virtualPath,
				.physicalPath    = source->path,
				.assetId         = includeAssetId,
				.rewrittenSource = *rewritten,
			};
			unit.includeIndexByInternalName.emplace(
				entry.internalName, unit.includeEntries.size()
			);
			unit.includeEntries.emplace_back(std::move(entry));
		}

		return unit;
	}

	std::optional<std::string> ShaderCompileUnitBuilder::RewriteSource(
		const AssetID sourceAssetId,
		const std::unordered_map<AssetID, std::string>& internalNames
	) const {
		const auto* source = mAssetManager.Get<ShaderSourceAssetData>(
			sourceAssetId
		);
		if (!source) {
			return std::nullopt;
		}

		std::string rewritten = source->sourceText;

		std::vector<const ResolvedShaderInclude*> replacements;
		replacements.reserve(source->resolvedIncludes.size());
		for (const ResolvedShaderInclude& include : source->resolvedIncludes) {
			if (!internalNames.contains(include.shaderSourceAssetId)) {
				Error(
					"ShaderCompileUnit",
					"Resolved include is absent from compile unit table: sourceAssetId={} includeAssetId={} include='{}'",
					sourceAssetId,
					include.shaderSourceAssetId,
					include.reference.path
				);
				return std::nullopt;
			}
			replacements.emplace_back(&include);
		}
		std::ranges::sort(
			replacements,
			[](const ResolvedShaderInclude* lhs,
			   const ResolvedShaderInclude* rhs) {
				return lhs->reference.sourceTokenBegin >
				       rhs->reference.sourceTokenBegin;
			}
		);

		for (const ResolvedShaderInclude* include : replacements) {
			const size_t begin = include->reference.sourceTokenBegin;
			const size_t end   = include->reference.sourceTokenEnd;
			if (begin >= end || end > rewritten.size()) {
				Error(
					"ShaderCompileUnit",
					"Shader include source range is invalid: sourceAssetId={} include='{}' begin={} end={} sourceSize={}",
					sourceAssetId,
					include->reference.path,
					begin,
					end,
					rewritten.size()
				);
				return std::nullopt;
			}
			rewritten.replace(
				begin,
				end - begin,
				"\"" + internalNames.at(include->shaderSourceAssetId) + "\""
			);
		}
		if (rewritten.starts_with("\xEF\xBB\xBF")) {
			rewritten.erase(0, 3);
		}

		const std::string diagnosticPath = BuildDiagnosticPath(sourceAssetId);
		return "#line 1 \"" + diagnosticPath + "\"\n" + rewritten;
	}

	std::string ShaderCompileUnitBuilder::BuildDiagnosticPath(
		const AssetID sourceAssetId
	) const {
		const auto* source = mAssetManager.Get<ShaderSourceAssetData>(
			sourceAssetId
		);
		if (!source) {
			return "unknown-shader-source";
		}
		std::string path = source->virtualPath.has_value() ?
			source->virtualPath->String() : source->path.ToGenericUtf8();
		std::string escaped;
		escaped.reserve(path.size());
		for (const char character : path) {
			if (character == '\\' || character == '"') {
				escaped.push_back('\\');
			}
			escaped.push_back(character);
		}
		return escaped;
	}
}
