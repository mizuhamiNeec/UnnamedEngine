#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>

#include <math/public/MathLib.h>

/// @class JsonWriter
/// @brief JSON構築用ユーティリティ (オブジェクト/配列のネストサポート)
class JsonWriter final {
	// ルート JSON 値
	nlohmann::json mRoot;
	// 出力パス
	std::string mPath;
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
	explicit JsonWriter(std::string path);
	void     BeginObject();
	void     EndObject();
	void     BeginArray();
	void     EndArray();
	void     Key(const std::string& key) const; // オブジェクト用キー

	template <typename T>
	void Write(const T& value) {
		// const 削除
		mStringCache.clear(); // キャッシュ無効化
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
					"Key() must be called before writing a value inside an object");
			}
			(*cur)[*mPendingKey] = value;
			mPendingKey.reset();
			return;
		}
		if (cur->is_array()) {
			cur->push_back(value);
			return;
		}
		throw std::runtime_error(
			"Current JSON container is neither object nor array");
	}

	bool                           Save() const;     // ファイルへ保存
	[[nodiscard]] std::string_view ToString() const; // 文字列取得 (必要時 dump)
};
