#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "core/filesystem/Path.h"

struct Vec4;
struct Vec3;

namespace Unnamed {
	/// @brief JSON書き込みクラス
	/// @details JSON形式のデータを構築し、ファイルに保存するためのクラスです。
	///          オブジェクトと配列のネストをサポートします。
	class JsonWriter final {
		// ルート JSON 値
		nlohmann::json mRoot;
		// 出力パス
		Path mPath;
		// 現在ネスト中のコンテナスタック
		mutable std::vector<nlohmann::json*> mStack;
		// オブジェクトに値を入れる前に指定されたキー (値書き込み待ち)
		mutable std::optional<std::string> mPendingKey;
		// dump キャッシュ
		mutable std::string mStringCache;

		// 現在のコンテナ取得 (無ければ nullptr)
		[[nodiscard]] nlohmann::json* Current() const {
			return mStack.empty() ? nullptr : mStack.back();
		}

		// コンテナ開始共通処理
		void BeginContainerInternal(nlohmann::json value);

	public:
		/// @brief コンストラクタ
		/// @param path 出力ファイルパス
		explicit JsonWriter(const char* path);
		/// @brief コンストラクタ
		/// @param path 出力ファイルパス
		explicit JsonWriter(const std::string& path);
		/// @brief コンストラクタ
		/// @param path 出力ファイルパス
		explicit JsonWriter(Path path);

		/// @brief オブジェクトの開始
		void BeginObject();

		/// @brief オブジェクトの終了
		void EndObject() const;

		/// @brief 配列の開始
		void BeginArray();

		/// @brief 配列の終了
		void EndArray() const;

		/// @brief オブジェクトのキーを設定する
		/// @param key キー名
		void Key(const std::string& key) const;

		template <typename T>
		void Write(const T& value) {
			mStringCache.clear();
			auto* cur = Current();
			if (!cur) {
				if (!mRoot.is_null()) {
					throw std::runtime_error("Root value already set");
				}
				mRoot = value;
				return;
			}
			if (cur->is_object()) {
				if (!mPendingKey.has_value()) {
					throw std::runtime_error(
						"Key() must be called before writing a value inside an object"
					);
				}
				(*cur)[*mPendingKey] = value;
				mPendingKey.reset();
				return;
			}
			if (cur->is_array()) {
				cur->emplace_back(value);
				return;
			}
			throw std::runtime_error(
				"Current JSON container is neither object nor array"
			);
		}

		void WriteVec3(const char* key, const Vec3& value);
		void WriteVec4(const char* key, const Vec4& value);

		/// @brief 構築されたJSONオブジェクトを取得する
		/// @return JSONオブジェクトへの定数参照
		[[nodiscard]] const nlohmann::json& GetRoot() const {
			return mRoot;
		}

		/// @brief ファイルに保存する
		/// @return 成功した場合true
		bool Save() const;

		/// @brief JSON文字列を取得する
		/// @return JSON文字列
		[[nodiscard]] std::string_view ToString() const;
	};
}
