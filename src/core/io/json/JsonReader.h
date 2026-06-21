#pragma once
#include <fstream>
#include <json.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief JSON読み込みクラス
	/// @details JSON形式のデータを読み込み、型安全にアクセスするためのラッパークラスです
	class JsonReader final {
	public:
		/// @brief デフォルトコンストラクタ
		JsonReader() = default;

		/// @brief JSONオブジェクトから初期化するコンストラクタ
		/// @param root JSONルートオブジェクト
		explicit JsonReader(const nlohmann::json& root)
			: mStorage(std::make_shared<nlohmann::json>(root)),
			  mNode(mStorage.get()),
			  mValid(true) {
		}

		/// @brief ファイルパスから読み込むコンストラクタ
		/// @param path JSONファイルのパス
		explicit JsonReader(const char* path) :
			JsonReader(Path(path == nullptr ? "" : path)) {
		}

		/// @brief ファイルパスから読み込むコンストラクタ
		/// @param path JSONファイルのパス
		explicit JsonReader(const std::string& path) :
			JsonReader(Path(path)) {
		}

		/// @brief ファイルパスから読み込むコンストラクタ
		/// @param path JSONファイルのパス
		explicit JsonReader(const Path& path) {
			std::ifstream ifs(path.Native());
			if (!ifs) {
				mValid = false;
				return;
			}
			try {
				mStorage = std::make_shared<nlohmann::json>();
				ifs >> *mStorage;
				mNode  = mStorage.get();
				mValid = true;
			} catch (...) {
				mStorage.reset();
				mNode  = nullptr;
				mValid = false;
			}
		}

		[[nodiscard]] bool Valid() const {
			return mValid && mNode;
		}

		[[nodiscard]] bool IsObject() const {
			return mNode && mNode->is_object();
		}

		[[nodiscard]] bool IsArray() const {
			return mNode && mNode->is_array();
		}

		[[nodiscard]] bool IsNumber() const {
			return mNode && mNode->is_number();
		}

		[[nodiscard]] bool IsString() const {
			return mNode && mNode->is_string();
		}

		[[nodiscard]] bool IsBoolean() const {
			return mNode && mNode->is_boolean();
		}

		[[nodiscard]] bool IsNumberInteger() const {
			return mNode && mNode->is_number_integer();
		}

		[[nodiscard]] bool IsNumberFloat() const {
			return mNode && mNode->is_number_float();
		}

		[[nodiscard]] bool Has(const std::string_view& key) const {
			return mNode && mNode->is_object() && mNode->contains(key);
		}

		// operator[] (オブジェクトキー)
		JsonReader operator[](const std::string_view& key) const {
			if (!Has(key)) {
				return {};
			}
			return JsonReader(mStorage, &(*mNode)[std::string(key)], mValid);
		}

		// operator[] (配列インデックス)
		JsonReader operator[](const size_t index) const {
			if (!mNode || !mNode->is_array() || index >= mNode->size()) {
				return {};
			}
			return JsonReader(mStorage, &(*mNode)[index], mValid);
		}

		// 配列サイズ
		[[nodiscard]] size_t Size() const {
			if (!mNode || !mNode->is_array()) {
				return 0;
			}
			return mNode->size();
		}

		[[nodiscard]] bool GetBool(const bool fallback = false) const {
			if (!mNode) {
				return fallback;
			}
			if (mNode->is_boolean()) {
				return mNode->get<bool>();
			}
			return fallback;
		}

		[[nodiscard]] std::string GetString(
			const std::string& fallback = "readerror"
		) const {
			if (!mNode) {
				return fallback;
			}
			if (mNode->is_string()) {
				return mNode->get<std::string>();
			}
			return fallback;
		}

		[[nodiscard]] float GetFloat(const float fallback = 0.0f) const {
			if (!mNode) {
				return fallback;
			}
			if (mNode->is_number_float() || mNode->is_number_integer() || mNode
			    ->
			    is_number_unsigned()) {
				return mNode->get<float>();
			}
			return fallback;
		}

		[[nodiscard]] int GetInt(const int fallback = 0) const {
			if (!mNode) {
				return fallback;
			}
			if (mNode->is_number_integer() || mNode->is_number_unsigned()) {
				return mNode->get<int>();
			}
			if (mNode->is_number_float()) {
				return static_cast<int>(mNode->get<float>());
			}
			return fallback;
		}

		[[nodiscard]] Vec2 GetVec2(const Vec2 fallback = Vec2::zero) const {
			if (!mNode || !mNode->is_array() || mNode->size() != 2) {
				return fallback;
			}
			try {
				return Vec2(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>()
				);
			} catch (...) {
				return fallback;
			}
		}

		[[nodiscard]] Vec3 GetVec3(const Vec3 fallback = Vec3::zero) const {
			if (!mNode || !mNode->is_array() || mNode->size() != 3) {
				return fallback;
			}
			try {
				return Vec3(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>(),
					mNode->at(2).get<float>()
				);
			} catch (...) {
				return fallback;
			}
		}

		[[nodiscard]] Vec4 GetVec4(const Vec4 fallback = Vec4::zero) const {
			if (!mNode || !mNode->is_array() || mNode->size() != 4) {
				return fallback;
			}
			try {
				return Vec4(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>(),
					mNode->at(2).get<float>(),
					mNode->at(3).get<float>()
				);
			} catch (...) {
				return fallback;
			}
		}

		[[nodiscard]] Quaternion GetQuaternion(
			const Quaternion fallback = Quaternion::identity
		) const {
			if (!mNode || !mNode->is_array() || mNode->size() != 4) {
				return fallback;
			}
			try {
				return Quaternion(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>(),
					mNode->at(2).get<float>(),
					mNode->at(3).get<float>()
				);
			} catch (...) {
				return fallback;
			}
		}

		[[nodiscard]] uint64_t GetUint64() const {
			return TryGetUint64().value_or(0ull);
		}

		/// @brief uint64 として厳密に取得する(変換できない場合は nullopt)
		/// @details 許容: unsigned整数, >=0 の signed整数, 0以上の float(整数部のみ), 10進文字列
		[[nodiscard]] std::optional<uint64_t> TryGetUint64() const {
			if (!mNode) {
				return std::nullopt;
			}

			try {
				if (mNode->is_number_unsigned()) {
					return mNode->get<uint64_t>();
				}
				if (mNode->is_number_integer()) {
					const auto s = mNode->get<int64_t>();
					if (s < 0) {
						return std::nullopt;
					}
					return static_cast<uint64_t>(s);
				}
				if (mNode->is_number_float()) {
					const auto d = mNode->get<double>();
					if (!(d >= 0.0)) {
						return std::nullopt;
					}
					if (d > static_cast<double>((std::numeric_limits<
						    uint64_t>::max)())) {
						return std::nullopt;
					}
					return static_cast<uint64_t>(d);
				}
				if (mNode->is_string()) {
					const auto&        s = mNode->get_ref<const std::string&>();
					size_t             idx = 0;
					unsigned long long v = std::stoull(s, &idx, 10);
					if (idx != s.size()) {
						return std::nullopt;
					}
					return v;
				}
			} catch (...) {
				return std::nullopt;
			}
			return std::nullopt;
		}

		[[nodiscard]] JsonReader GetArray() const {
			return *this;
		}

		template <typename T>
		std::optional<T> Read(const std::string_view& key) const {
			if (!Has(key)) {
				return std::nullopt;
			}
			try {
				return (*mNode)[std::string(key)].get<T>();
			} catch (...) {
				return std::nullopt;
			}
		}

		/// @brief uint64 をキーから取得する(失敗時は nullopt)
		[[nodiscard]] std::optional<uint64_t> ReadUint64(
			const std::string_view& key
		) const {
			const JsonReader v = (*this)[key];
			if (!v.Valid()) {
				return std::nullopt;
			}
			return v.TryGetUint64();
		}

		template <typename T>
		std::vector<T> ReadArray(const std::string_view& key) const {
			std::vector<T> out;
			if (!Has(key)) {
				return out;
			}
			const auto& j = (*mNode)[std::string(key)];
			if (!j.is_array()) {
				return out;
			}
			out.reserve(j.size());
			for (auto& v : j) {
				try {
					out.emplace_back(v.get<T>());
				} catch (...) {
					/* skip */
				}
			}
			return out;
		}

		template <typename Fn>
		void ForEachObject(Fn&& fn) const {
			if (!mNode || !mNode->is_object()) {
				return;
			}
			for (auto it = mNode->begin(); it != mNode->end(); ++it) {
				fn(it.key(), JsonReader(mStorage, &it.value(), mValid));
			}
		}

	private:
		JsonReader(
			std::shared_ptr<nlohmann::json> storage,
			const nlohmann::json*           node, const bool valid
		)
			: mStorage(std::move(storage)),
			  mNode(node),
			  mValid(valid && node != nullptr) {
		}

		std::shared_ptr<nlohmann::json> mStorage;
		const nlohmann::json*           mNode{nullptr};
		bool                            mValid{false};
	};
}
