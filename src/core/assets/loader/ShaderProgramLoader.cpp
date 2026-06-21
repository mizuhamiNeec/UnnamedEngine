#include "ShaderProgramLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	class JsonReader;

	namespace {
		bool IsShaderProgramPath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".shader.json"
			);
		}

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

		std::optional<ShaderProgramStage> ParseStage(
			const JsonReader& j, const Path& baseDir
		) {
			if (!j.Valid() || !j.IsObject()) {
				return std::nullopt;
			}

			const auto sourcePath = j.Read<std::string>("path");
			if (!sourcePath.has_value() || sourcePath->empty()) {
				return std::nullopt;
			}

			ShaderProgramStage stage = {};
			stage.sourcePath         = Path::ResolveRelativePath(
				baseDir.Native(),
				*sourcePath
			);
			stage.entry   = j.Read<std::string>("entry").value_or("Main");
			stage.profile = j.Read<std::string>("profile").value_or(
				std::string()
			);
			if (j.Has("defines")) {
				ParseDefines(j["defines"], stage.defines);
			}
			return stage;
		}
	}

	ShaderProgramLoader::ShaderProgramLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {
	}

	bool ShaderProgramLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = IsShaderProgramPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::SHADER_PROGRAM : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult ShaderProgramLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full    = path.LexicallyNormal();
		const Path baseDir = full.ParentPath();

		ShaderProgramAssetData data = {};
		data.name                   = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		if (root.Has("vs")) {
			data.vs = ParseStage(root["vs"], baseDir);
		}
		if (root.Has("ps")) {
			data.ps = ParseStage(root["ps"], baseDir);
		}
		if (root.Has("cs")) {
			data.cs = ParseStage(root["cs"], baseDir);
		}

		const JsonReader includeDirs = root["includeDirs"];
		if (includeDirs.Valid() && includeDirs.IsArray()) {
			for (size_t i = 0; i < includeDirs.Size(); ++i) {
				const JsonReader v = includeDirs[i];
				if (!v.Valid() || !v.IsString()) {
					continue;
				}
				data.includeDirectories.emplace_back(
					Path::ResolveRelativePath(baseDir.Native(), v.GetString())
				);
			}
		}

		auto addStageDependency = [&](
			const std::optional<ShaderProgramStage>& stage
		) {
			if (!stage.has_value()) {
				return;
			}
			const AssetID dep = mAssetManager->LoadFromFile(
				stage->sourcePath, ASSET_TYPE::SHADER_SOURCE
			);
			if (dep != kInvalidAssetID) {
				result.dependencies.emplace_back(dep);
			}
		};

		addStageDependency(data.vs);
		addStageDependency(data.ps);
		addStageDependency(data.cs);

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
