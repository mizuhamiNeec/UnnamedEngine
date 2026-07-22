#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace Unnamed::SceneFolderPath {
	/// @brief シーン内フォルダパスを構成要素へ分割します。
	/// @param folderPath 分割するフォルダパス
	/// @return 空要素を除いたフォルダ名配列
	[[nodiscard]] inline std::vector<std::string> Split(
		const std::string_view folderPath
	) {
		std::vector<std::string> parts;
		size_t                   begin = 0;
		while (begin < folderPath.size()) {
			const size_t end = folderPath.find('/', begin);
			const size_t len = (end == std::string_view::npos ?
				                    folderPath.size() :
				                    end) - begin;
			if (len > 0) {
				parts.emplace_back(folderPath.substr(begin, len));
			}
			if (end == std::string_view::npos) {
				break;
			}
			begin = end + 1;
		}
		return parts;
	}

	/// @brief シーン内フォルダパスを区切り文字'/'、先頭/末尾スラッシュなしの形式に正規化します。
	/// @param folderPath 正規化するフォルダパス
	/// @return 正規化済みフォルダパス
	[[nodiscard]] inline std::string Normalize(
		const std::string_view folderPath
	) {
		std::string path(folderPath);
		std::replace(path.begin(), path.end(), '\\', '/');
		while (!path.empty() && path.front() == '/') {
			path.erase(path.begin());
		}
		while (!path.empty() && path.back() == '/') {
			path.pop_back();
		}
		return path;
	}

	/// @brief 親フォルダと子名をシーン内フォルダパスとして結合します。
	/// @param parent 親フォルダパス
	/// @param child 子フォルダ名または相対パス
	/// @return 結合したフォルダパス
	[[nodiscard]] inline std::string Join(
		const std::string_view parent, const std::string_view child
	) {
		if (parent.empty()) {
			return std::string(child);
		}
		if (child.empty()) {
			return std::string(parent);
		}
		return std::string(parent) + "/" + std::string(child);
	}

	/// @brief シーン内フォルダパスから親フォルダパスを取得します。
	/// @param folderPath 対象フォルダパス
	/// @return 親フォルダパス。親がない場合は空文字列
	[[nodiscard]] inline std::string ParentPath(
		const std::string_view folderPath
	) {
		const size_t slash = folderPath.find_last_of('/');
		return slash == std::string_view::npos ?
			       std::string() :
			       std::string(folderPath.substr(0, slash));
	}

	/// @brief シーン内フォルダパスから末尾のフォルダ名を取得します。
	/// @param folderPath 対象フォルダパス
	/// @return 末尾のフォルダ名
	[[nodiscard]] inline std::string LeafName(
		const std::string_view folderPath
	) {
		const size_t slash = folderPath.find_last_of('/');
		return slash == std::string_view::npos ?
			       std::string(folderPath) :
			       std::string(folderPath.substr(slash + 1));
	}

	/// @brief pathがancestor自身、またはancestor配下のフォルダかを判定します。
	/// @param path 判定するフォルダパス
	/// @param ancestor 祖先候補のフォルダパス
	/// @return pathがancestor自身または配下ならtrue
	[[nodiscard]] inline bool IsEqualOrDescendant(
		const std::string_view path, const std::string_view ancestor
	) {
		if (ancestor.empty()) {
			return true;
		}
		if (path == ancestor) {
			return true;
		}
		if (path.size() <= ancestor.size()) {
			return false;
		}
		return path.substr(0, ancestor.size()) == ancestor &&
		       path[ancestor.size()] == '/';
	}

	/// @brief sourceフォルダ配下のpathをdestination配下へ付け替えます。
	/// @param path 置換対象のフォルダパス
	/// @param source 置換元フォルダパス
	/// @param destination 置換先フォルダパス
	/// @return source配下であればprefixを置換したパス、それ以外はpath
	[[nodiscard]] inline std::string ReplacePrefix(
		const std::string_view path,
		const std::string_view source,
		const std::string_view destination
	) {
		if (!IsEqualOrDescendant(path, source)) {
			return std::string(path);
		}

		const std::string_view suffix = path.size() == source.size() ?
			                                std::string_view{} :
			                                path.substr(source.size() + 1);
		if (destination.empty()) {
			return std::string(suffix);
		}
		if (suffix.empty()) {
			return std::string(destination);
		}
		return std::string(destination) + "/" + std::string(suffix);
	}
}
