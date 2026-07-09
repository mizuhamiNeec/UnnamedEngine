#include "ShaderProgramLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/VirtualPath.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	class JsonReader;

	namespace {
		void ParseDefines(
			const JsonReader&                                 j,
			std::vector<std::pair<std::string, std::string>>& outDefines
		) {
			if (j.IsArray()) {
				for (size_t i = 0; i < j.Size(); ++i) {
					const JsonReader v = j[i];
					if (!v.Valid() || !v.IsString()) {
						continue;
					}
					const std::string s  = v.GetString();
					const size_t      eq = s.find('=');
					if (eq == std::string::npos) {
						outDefines.emplace_back(s, "1");
					} else {
						outDefines.emplace_back(
							s.substr(0, eq), s.substr(eq + 1)
						);
					}
				}
				return;
			}

			if (!j.IsObject()) {
				return;
			}
			j.ForEachObject(
				[&outDefines](const std::string& k, const JsonReader& v) {
					if (v.IsString()) {
						outDefines.emplace_back(k, v.GetString());
					} else if (v.IsNumberInteger()) {
						outDefines.emplace_back(k, std::to_string(v.GetInt()));
					} else if (v.IsNumberFloat()) {
						outDefines.emplace_back(
							k, std::to_string(v.GetFloat())
						);
					} else if (v.IsBoolean()) {
						outDefines.emplace_back(k, v.GetBool() ? "1" : "0");
					}
				}
			);
		}

		bool ParseStage(
			const JsonReader& j,
			const Path&       shaderProgramPath,
			const std::string_view fieldName,
			ShaderProgramStage& output
		) {
			if (!j.Valid() || !j.IsObject()) {
				Error(
					"ShaderProgramLoader",
					"Invalid shader stage type: shaderProgram='{}' field='{}' expected='object'",
					shaderProgramPath,
					fieldName
				);
				return false;
			}

			const JsonReader sourcePathNode = j["path"];
			if (!sourcePathNode.Valid() || !sourcePathNode.IsString()) {
				Error(
					"ShaderProgramLoader",
					"Invalid shader stage source type: shaderProgram='{}' field='{}.path' expected='string'",
					shaderProgramPath,
					fieldName
				);
				return false;
			}

			const std::string sourcePathText = sourcePathNode.GetString();
			const std::optional<VirtualPath> sourcePath =
				VirtualPath::ParseContentReference(sourcePathText);
			if (!sourcePath.has_value()) {
				Error(
					"ShaderProgramLoader",
					"Invalid shader stage source virtual path: shaderProgram='{}' field='{}.path' virtualPath='{}'",
					shaderProgramPath,
					fieldName,
					sourcePathText
				);
				return false;
			}

			output.sourcePath = *sourcePath;
			output.entry = j.Read<std::string>("entry").value_or("Main");
			output.profile = j.Read<std::string>("profile").value_or(
				std::string()
			);
			if (j.Has("defines")) {
				ParseDefines(j["defines"], output.defines);
			}
			return true;
		}
	}

	ShaderProgramLoader::ShaderProgramLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {
	}

	bool ShaderProgramLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = StrUtil::EndsWithIgnoreCase(
			path.ToGenericUtf8(), ".shader.json"
		);
		if (outType) {
			*outType = ok ? ASSET_TYPE::SHADER_PROGRAM : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult ShaderProgramLoader::Load(const Path& path) {
		return Load(path, AssetLoadContext{});
	}

	LoadResult ShaderProgramLoader::Load(
		const Path& path, const AssetLoadContext& context
	) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full = path.LexicallyNormal();
		if (root.Has("includeDirs")) {
			Error(
				"ShaderProgramLoader",
				"ShaderProgram includeDirs is not supported; use explicit HLSL include references: shaderProgram='{}'",
				full
			);
			return result;
		}

		ShaderProgramAssetData data = {};
		data.name                   = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		auto loadStage = [this, &context, &full, &result](
			const JsonReader& stageNode,
			const std::string_view fieldName,
			std::optional<ShaderProgramStage>& output
		) {
			ShaderProgramStage stage = {};
			if (!ParseStage(stageNode, full, fieldName, stage)) {
				return false;
			}

			stage.shaderSourceAssetId = context.resolvedMountId.empty()
				? mAssetManager->LoadAsset(
					stage.sourcePath, ASSET_TYPE::SHADER_SOURCE)
				: mAssetManager->LoadAssetFromMount(
					stage.sourcePath,
					context.resolvedMountId,
					ASSET_TYPE::SHADER_SOURCE
				);
			if (stage.shaderSourceAssetId == kInvalidAssetID) {
				const auto candidate = context.resolvedMountId.empty() ?
					mAssetManager->GetContentPathResolver().ResolveFile(
						stage.sourcePath
					) :
					mAssetManager->GetContentPathResolver().
						BuildFileCandidateFromMount(
							context.resolvedMountId, stage.sourcePath
						);
				if (candidate.has_value()) {
					const AssetID failedDependency = mAssetManager->FindByPath(
						candidate->resolvedPath
					);
					if (failedDependency != kInvalidAssetID) {
						result.dependencies.emplace_back(failedDependency);
					}
				}
				Error(
					"ShaderProgramLoader",
					"Shader stage source dependency load failed: shaderProgram='{}' field='{}.path' virtualPath='{}' mount='{}'",
					full,
					fieldName,
					stage.sourcePath.String(),
					context.resolvedMountId
				);
				return false;
			}

			result.dependencies.emplace_back(stage.shaderSourceAssetId);
			output = std::move(stage);
			return true;
		};

		if (root.Has("vs") && !loadStage(root["vs"], "vs", data.vs)) {
			return result;
		}
		if (root.Has("ps") && !loadStage(root["ps"], "ps", data.ps)) {
			return result;
		}
		if (root.Has("cs") && !loadStage(root["cs"], "cs", data.cs)) {
			return result;
		}

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(full.Stem().Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return result;
	}
}
