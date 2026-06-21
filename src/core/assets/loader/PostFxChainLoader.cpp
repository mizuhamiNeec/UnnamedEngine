#include "PostFxChainLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/PostFxChainAssetData.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		/// @brief パスがPostFxChainアセットとして適切かどうかを判定する。拡張子ベースで判定する。
		/// @param path 判定するパス
		/// @return パスがPostFxChainアセットとして適切であればtrue、そうでなければfalse
		bool IsPostFxPath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".postfx.json"
			);
		}
	}

	PostFxChainLoader::PostFxChainLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {}

	bool PostFxChainLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = IsPostFxPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::POST_FX_CHAIN : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult PostFxChainLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full    = path.LexicallyNormal();
		const Path baseDir = full.ParentPath();

		PostFxChainAssetData data = {};
		data.name                 = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		const JsonReader passes = root["passes"];
		if (passes.Valid() && passes.IsArray()) {
			for (size_t i = 0; i < passes.Size(); ++i) {
				const JsonReader p = passes[i];
				if (!p.Valid() || !p.IsObject()) {
					continue;
				}

				PostFxPassAssetData pass = {};
				pass.name = p.Read<std::string>("name").value_or("Pass");
				pass.enabled = p.Read<bool>("enabled").value_or(true);

				if (const auto shaderPath = p.Read<std::string>("shader");
					shaderPath.has_value() && !shaderPath->empty()) {
					pass.shaderProgramPath = Path::ResolveRelativePath(
						baseDir.Native(),
						*shaderPath
					);
					pass.shaderProgramId = mAssetManager->LoadFromFile(
						pass.shaderProgramPath,
						ASSET_TYPE::SHADER_PROGRAM
					);
					if (pass.shaderProgramId != kInvalidAssetID) {
						result.dependencies.emplace_back(pass.shaderProgramId);
					}
				}

				const JsonReader scalars = p["scalars"];
				if (scalars.Valid() && scalars.IsObject()) {
					scalars.ForEachObject(
						[&pass](const std::string& k, const JsonReader& v) {
							if (!v.IsNumber()) {
								return;
							}
							pass.scalarParams[k] = v.GetFloat();
						}
					);
				}

				const JsonReader colors = p["colors"];
				if (colors.Valid() && colors.IsObject()) {
					colors.ForEachObject(
						[&pass](const std::string& k, const JsonReader& v) {
							if (!v.IsArray() || v.Size() != 4) {
								return;
							}
							pass.colorParams[k] = v.GetVec4();
						}
					);
				}

				data.passes.emplace_back(std::move(pass));
			}
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
