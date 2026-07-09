#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class MeshAssetLoader final : public IAssetLoader {
	public:
		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;

	private:
		/// @brief 元のアセットファイルのパスから派生キャッシュファイルのパスを生成する。派生キャッシュは、元のファイルの状態を表すスタンプとインポート設定のハッシュを組み合わせて、元のファイルが同一かどうかを判定できるようにする。
		/// @param sourcePath 元のアセットファイルのパス
		/// @return 派生キャッシュファイルのパス
		static Path GetDerivedCachePath(const Path& sourcePath);

		/// @brief 派生キャッシュを読み込む。キャッシュが存在しない、古い、または不正な場合はfalseを返す。
		/// @param path 元のアセットファイルのパス
		/// @param out 読み込んだキャッシュの内容を格納する出力パラメータ
		/// @return キャッシュが有効であればtrue、そうでなければfalse
		static bool TryLoadDerivedCache(const Path& path, LoadResult& out);

		/// @brief 派生キャッシュを書き込む。書き込みに失敗した場合はfalseを返す。
		/// @param path 元のアセットファイルのパス
		/// @param in 書き込む内容を格納する入力パラメータ
		/// @return 書き込みに成功すればtrue、そうでなければfalse
		static bool WriteDerivedCache(const Path& path, const LoadResult& in);
	};
}
