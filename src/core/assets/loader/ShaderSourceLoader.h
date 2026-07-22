#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	/// @brief シェーダーソースローダークラス
	class ShaderSourceLoader : public IAssetLoader {
	public:
		/// @brief コンストラクタ
		/// @param assetManager アセットマネージャーの参照
		explicit ShaderSourceLoader(AssetManager* assetManager);

		/// @brief 対象のファイルを読み込めるか?
		/// @param path 読み込むファイルのパス
		/// @param outType ロード可能な場合、対応するASSET_TYPEを出力するポインタ
		/// @return 読み込める場合はtrue、そうでない場合はfalse
		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;

		/// @brief ファイルを読み込む
		/// @param path 読み込むファイルのパス
		/// @return ロード結果
		LoadResult Load(const Path& path) override;

		/// @brief mount情報を伴ってShaderSourceを読み込みます。
		/// @param path 解決済み物理パス。
		/// @param context 親Assetから継承されたmount情報。
		/// @return ロード結果。
		LoadResult Load(
			const Path& path, const AssetLoadContext& context
		) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
