#include "MaterialInstanceAssetLoader.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialInstanceAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		/// @brief マテリアルインスタンスアセットのパスか?
		/// @param path パス
		/// @return マテリアルインスタンスアセットのパスならtrue
		bool IsMaterialInstancePath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".matinst.json"
			);
		}
	}

	MaterialInstanceAssetLoader::MaterialInstanceAssetLoader(
		AssetManager* assetManager
	) : mAssetManager(assetManager) {}

	bool MaterialInstanceAssetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const bool ok = IsMaterialInstancePath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL_INSTANCE : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialInstanceAssetLoader::Load(const std::string& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const std::filesystem::path full = Path::FromUtf8(path);
		const std::filesystem::path baseDir = full.parent_path();

		// "name" フィールドがあればそれを、なければファイル名をアセット名とする。
		MaterialInstanceAssetData data = {};
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.filename())
		);

		// "material" フィールドがあればマテリアルアセットを読み込む。
		if (const auto materialPath = root.Read<std::string>("material");
			materialPath.has_value() && !materialPath->empty()) {
			data.materialPath = Path::ResolveRelativePath(baseDir, *materialPath);
			data.materialId   = mAssetManager->LoadFromFile(
				data.materialPath, ASSET_TYPE::MATERIAL
			);
			if (data.materialId != kInvalidAssetID) {
				result.dependencies.emplace_back(data.materialId);
			}
		}

		// "textures" フィールドがあればテクスチャオーバーライドを読み込む。
		const JsonReader textures = root["textures"];
		if (textures.Valid() && textures.IsObject()) {
			textures.ForEachObject(
				[&](
				const std::string& slot, const JsonReader& texturePathNode
			) {
					if (!texturePathNode.IsString()) {
						return;
					}
					std::string texturePath = Path::ResolveRelativePath(
						baseDir, texturePathNode.GetString()
					);
					data.textureOverrides[slot] = texturePath;

					const AssetID textureDep = mAssetManager->LoadFromFile(
						texturePath, ASSET_TYPE::TEXTURE
					);
					if (textureDep != kInvalidAssetID) {
						result.dependencies.emplace_back(textureDep);
					}
				}
			);
		}

		// "scalars" フィールドがあればスカラーオーバーライドを読み込む。
		const JsonReader scalars = root["scalars"];
		if (scalars.Valid() && scalars.IsObject()) {
			scalars.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					if (!v.IsNumber()) {
						return;
					}
					data.scalarOverrides[k] = v.GetFloat();
				}
			);
		}

		// "vectors" フィールドがあればベクターオーバーライドを読み込む。
		const JsonReader vectors = root["vectors"];
		if (vectors.Valid() && vectors.IsObject()) {
			vectors.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					data.vectorOverrides[k] = v.GetVec4(Vec4(0, 0, 0, 0));
				}
			);
		}

		// payloadにデータをセット
		result.payload = std::move(data);

		// アセット名が指定されていない場合は、ファイル名から拡張子を除いたものをアセット名とする。
		result.resolveName = Path::ToUtf8String(full.stem().stem());

		std::error_code ec;
		if (Path::ExistsUtf8(path, ec)) {
			result.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
		}
		return result;
	}
}
