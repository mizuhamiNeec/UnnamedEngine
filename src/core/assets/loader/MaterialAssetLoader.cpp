#include "MaterialAssetLoader.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	MaterialAssetLoader::MaterialAssetLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {
	}

	bool MaterialAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const bool ok = StrUtil::EndsWithIgnoreCase(
			path.ToGenericUtf8(),
			".material.json"
		);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialAssetLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full = path.LexicallyNormal();

		MaterialAssetData data = {};

		// "name" フィールドがあればそれを、なければファイル名をアセット名とする。
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		// "domain" フィールドがあればそれを、なければ "pbr" をドメインとして扱う。
		data.domain = ParseMaterialDomain(
			root.Read<std::string>("domain").value_or("pbr")
		);
		if (const auto shadingModel = root.Read<std::string>("shadingModel")) {
			data.shadingModel = ParseMaterialShadingModel(*shadingModel);
		} else if (data.domain == MATERIAL_DOMAIN::UNLIT) {
			data.shadingModel = MATERIAL_SHADING_MODEL::UNLIT;
		}

		// ShaderProgramはMaterialの成立に必須。
		const JsonReader shaderPathNode = root["shader"];
		if (!shaderPathNode.Valid() || !shaderPathNode.IsString()) {
			Error(
				"MaterialLoader",
				"Invalid shader reference type: material='{}' field='shader' expected='string'",
				full
			);
			return result;
		}

		const std::string shaderPathText = shaderPathNode.GetString();
		const std::optional<VirtualPath> shaderPath =
			VirtualPath::ParseContentReference(shaderPathText);
		if (!shaderPath.has_value()) {
			Error(
				"MaterialLoader",
				"Invalid shader program virtual path: material='{}' field='shader' virtualPath='{}'",
				full,
				shaderPathText
			);
			return result;
		}

		data.shaderProgramPath = *shaderPath;
		data.shaderProgramId   = mAssetManager->LoadAsset(
			*shaderPath,
			ASSET_TYPE::SHADER_PROGRAM
		);
		if (data.shaderProgramId == kInvalidAssetID) {
			Error(
				"MaterialLoader",
				"Shader program dependency load failed: material='{}' field='shader' virtualPath='{}'",
				full,
				shaderPath->String()
			);
			return result;
		}
		result.dependencies.emplace_back(data.shaderProgramId);

		// "renderState" フィールドがあればレンダーステートを読み込む。
		const JsonReader rs = root["renderState"];
		if (rs.Valid() && rs.IsObject()) {
			data.renderState.depthEnable =
				rs.Read<bool>("depthEnable").value_or(true);
			data.renderState.depthWrite =
				rs.Read<bool>("depthWrite").value_or(true);
			data.renderState.cullBackFace =
				rs.Read<bool>("cullBackFace").value_or(true);
			data.renderState.blendEnable =
				rs.Read<bool>("blendEnable").value_or(false);
			data.renderState.castsShadow =
				rs.Read<bool>("castsShadow").value_or(true);
			data.renderState.shadowCullMode = ParseMaterialShadowCullMode(
				rs.Read<std::string>("shadowCullMode").value_or(
					"FollowMaterial"
				)
			);
			data.renderState.stencilEnable =
				rs.Read<bool>("stencilEnable").value_or(false);
			data.renderState.stencilReadMask = static_cast<uint8_t>(
				rs.Read<int>("stencilReadMask").value_or(255)
			);
			data.renderState.stencilWriteMask = static_cast<uint8_t>(
				rs.Read<int>("stencilWriteMask").value_or(255)
			);
		}

		// "scalars" フィールドがあればスカラー型のマテリアルパラメータを読み込む。
		const JsonReader scalars = root["scalars"];
		if (scalars.Valid() && scalars.IsObject()) {
			scalars.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					if (!v.IsNumber()) {
						return;
					}
					data.scalarParams[k] = v.GetFloat();
				}
			);
		}

		// "vectors" フィールドがあればベクトル型のマテリアルパラメータを読み込む。
		const JsonReader vectors = root["vectors"];
		if (vectors.Valid() && vectors.IsObject()) {
			vectors.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					data.vectorParams[k] = v.GetVec4(Vec4(0, 0, 0, 0));
				}
			);
		}

		// payloadにデータをセット
		result.payload = std::move(data);

		// 解決名は拡張子を取り除いたものを使う(.material.jsonと2段界)
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
