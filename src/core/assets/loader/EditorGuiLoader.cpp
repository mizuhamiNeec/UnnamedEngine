#include "EditorGuiLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "EdGuiLdr";

	namespace {
		/// @brief パスがEditorGuiアセットとして適切かどうかを判定します。
		/// @param path 判定するパス
		/// @return パスがEditorGuiアセットとして適切か?
		bool IsEditorGuiPath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".edgui.lua"
			);
		}
	}

	bool EditorGuiLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = IsEditorGuiPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::EDITOR_GUI : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult EditorGuiLoader::Load(const Path& path) {
		LoadResult result = {};

		EditorGuiData data = {};
		data.sourcePath    = path.LexicallyNormal(); // ソースファイルのパスを保存

		std::string source;
		if (!StrUtil::ReadFileToString(path, source)) {
			Error(kChannel, "エディターGUIの読み込みに失敗しました: {}", path);
			data.lastError =
				"Failed to read file: " + path.ToGenericUtf8(); // エラー内容を保存
			data.hasError = true; // エラーが発生したことを示すフラグを立てる
			return result;
		}

		data.source = source; // ソースコードを保存

		Msg(
			kChannel,
			"Loaded Editor GUI: \n {}",
			data.source
		);

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(path.FileName());
		if (std::error_code ec;
			std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return result;
	}
}
