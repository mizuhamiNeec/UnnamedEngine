#include "MaterialInstanceAssetLoader.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialInstanceAssetData.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		/// @brief マテリアルインスタンスアセットのパスか?
		/// @param path パス
		/// @return マテリアルインスタンスアセットのパスならtrue
		bool IsMaterialInstancePath(const Path& path) {
			return StrUtil::EndsWithIgnoreCase(
				path.ToGenericUtf8(),
				".matinst.json"
			);
		}
	}

	MaterialInstanceAssetLoader::MaterialInstanceAssetLoader(
		AssetManager* assetManager
	) : mAssetManager(assetManager) {
	}

	bool MaterialInstanceAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const bool ok = IsMaterialInstancePath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL_INSTANCE : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialInstanceAssetLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full = path.LexicallyNormal();

		// "name" フィールドがあればそれを、なければファイル名をアセット名とする。
		MaterialInstanceAssetData data = {};
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		// 基底MaterialはMaterial Instanceの成立に必須。
		const JsonReader materialPathNode = root["material"];
		if (!materialPathNode.Valid() || !materialPathNode.IsString()) {
			Error(
				"MaterialInstanceLoader",
				"Invalid material reference type: materialInstance='{}' field='material' expected='string'",
				full
			);
			return result;
		}

		const std::string materialPathText = materialPathNode.GetString();
		const std::optional<VirtualPath> materialPath =
			VirtualPath::ParseContentReference(materialPathText);
		if (!materialPath.has_value()) {
			Error(
				"MaterialInstanceLoader",
				"Invalid material virtual path: materialInstance='{}' field='material' virtualPath='{}'",
				full,
				materialPathText
			);
			return result;
		}

		data.materialPath = *materialPath;
		data.materialId   = mAssetManager->LoadAsset(
			*materialPath,
			ASSET_TYPE::MATERIAL
		);
		if (data.materialId == kInvalidAssetID) {
			Error(
				"MaterialInstanceLoader",
				"Material dependency load failed: materialInstance='{}' field='material' virtualPath='{}'",
				full,
				materialPath->String()
			);
			return result;
		}
		result.dependencies.emplace_back(data.materialId);

		// "textures" フィールドがあればテクスチャオーバーライドを読み込む。
		const JsonReader textures = root["textures"];
		if (textures.Valid() && !textures.IsObject()) {
			Error(
				"MaterialInstanceLoader",
				"Invalid texture references type: materialInstance='{}' field='textures' expected='object'",
				full
			);
			return result;
		}
		if (textures.Valid()) {
			bool textureReferencesValid = true;
			textures.ForEachObject(
				[&](
				const std::string& slot, const JsonReader& texturePathNode
			) {
					if (!texturePathNode.IsString()) {
						Error(
							"MaterialInstanceLoader",
							"Invalid texture reference type: materialInstance='{}' field='textures.{}' expected='string'",
							full,
							slot
						);
						textureReferencesValid = false;
						return;
					}

					const std::string texturePathText =
						texturePathNode.GetString();
					const std::optional<VirtualPath> texturePath =
						VirtualPath::ParseContentReference(texturePathText);
					if (!texturePath.has_value()) {
						Error(
							"MaterialInstanceLoader",
							"Invalid texture virtual path: materialInstance='{}' field='textures.{}' virtualPath='{}'",
							full,
							slot,
							texturePathText
						);
						textureReferencesValid = false;
						return;
					}

					const AssetID textureAssetId = mAssetManager->LoadTexture(
						*texturePath
					);
					if (textureAssetId == kInvalidAssetID) {
						Error(
							"MaterialInstanceLoader",
							"Texture dependency load failed: materialInstance='{}' field='textures.{}' virtualPath='{}'",
							full,
							slot,
							texturePath->String()
						);
						textureReferencesValid = false;
						return;
					}

					data.textureOverrides.insert_or_assign(
						slot,
						MatTextureOverride{
							.assetPath = *texturePath,
							.assetId   = textureAssetId,
						}
					);
					result.dependencies.emplace_back(textureAssetId);
				}
			);
			if (!textureReferencesValid) {
				return {};
			}
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
					data.vectorOverrides[k] = v.GetVec4(Vec4::zero);
				}
			);
		}

		// payloadにデータをセット
		result.payload = std::move(data);

		// アセット名が指定されていない場合は、ファイル名から拡張子を除いたものをアセット名とする。
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
