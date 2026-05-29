#include "MaterialAssetLoader.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		/// @brief パスがマテリアルアセットのものであるか?
		/// @param path 判定するパス
		/// @return マテリアルアセットのパスであればtrue
		bool IsMaterialPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".material.json"
			);
		}

		/// @brief マテリアルドメインを文字列から解析する
		/// @param s 解析する文字列
		/// @return 解析されたマテリアルドメイン。解析できない場合はPBR_METAL_ROUGHを返す。
		MATERIAL_DOMAIN ParseDomain(const std::string& s) {
			const auto v = StrUtil::ToLowerCase(s);
			if (v == "unlit") {
				return MATERIAL_DOMAIN::UNLIT;
			}
			return MATERIAL_DOMAIN::PBR_METAL_ROUGH;
		}
	}

	MaterialAssetLoader::MaterialAssetLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {}

	bool MaterialAssetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const bool ok = IsMaterialPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialAssetLoader::Load(const std::string& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const std::filesystem::path full = Path::FromUtf8(path);
		const std::filesystem::path baseDir = full.parent_path();

		MaterialAssetData data = {};

		// "name" フィールドがあればそれを、なければファイル名をアセット名とする。
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.filename())
		);

		// "domain" フィールドがあればそれを、なければ "pbr" をドメインとして扱う。
		data.domain = ParseDomain(
			root.Read<std::string>("domain").value_or("pbr")
		);

		// "shader" フィールドがあればシェーダープログラムを読み込む。
		if (const auto shader = root.Read<std::string>("shader");
			shader.has_value() && !shader->empty()) {
			data.shaderProgramPath = Path::ResolveRelativePath(baseDir, *shader);
			data.shaderProgramId   = mAssetManager->LoadFromFile(
				data.shaderProgramPath, ASSET_TYPE::SHADER_PROGRAM
			);
			if (data.shaderProgramId != kInvalidAssetID) {
				result.dependencies.emplace_back(data.shaderProgramId);
			}
		}

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
		result.resolveName = Path::ToUtf8String(full.stem().stem());

		std::error_code ec;
		if (Path::ExistsUtf8(path, ec)) {
			result.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
		}
		return result;
	}
}
