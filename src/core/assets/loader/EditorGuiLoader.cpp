#include "EditorGuiLoader.h"

#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "EdGuiLdr";

	namespace {
		/// @brief パスがEditorGuiアセットとして適切かどうかを判定します。
		/// @param path 判定するパス
		/// @return パスがEditorGuiアセットとして適切か?
		bool IsEditorGuiPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".edgui.lua"
			);
		}
	}

	bool EditorGuiLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsEditorGuiPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::EDITOR_GUI : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult EditorGuiLoader::Load(const std::string& path) {
		LoadResult result = {};

		EditorGuiData data = {};
		data.sourcePath    = path; // ソースファイルのパスを保存

		std::string source;
		if (!StrUtil::ReadFileToString(path, source)) {
			Error(kChannel, "エディターGUIの読み込みに失敗しました: {}", path);
			data.lastError = "Failed to read file: " + path; // エラー内容を保存
			data.hasError  = true; // エラーが発生したことを示すフラグを立てる
			return result;
		}

		data.source = source; // ソースコードを保存

		Msg(
			kChannel,
			"Loaded Editor GUI: \n {}",
			data.source
		);

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(
			Path::FromUtf8(path).filename()
		);
		if (std::error_code ec;
			Path::ExistsUtf8(path, ec)) {
			result.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
		}

		return result;
	}
}
