#pragma once
#include <json.hpp>
#include <optional>
#include <string_view>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

/// @class JsonReader
class JsonReader final {
public:
	JsonReader() = default;

	explicit JsonReader(const nlohmann::json& root)
		: mStorage(std::make_shared<nlohmann::json>(root)),
		  mNode(mStorage.get()), mValid(true) {
	}

	explicit JsonReader(const std::string& path) {
		std::ifstream ifs(path);
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

	[[nodiscard]] bool Valid() const { return mValid && mNode; }

	[[nodiscard]] bool Has(const std::string_view& key) const {
		return mNode && mNode->is_object() && mNode->contains(key);
	}
	
	// operator[] (オブジェクトキー)
	JsonReader operator[](const std::string_view& key) const {
		if (!Has(key)) { return {}; }
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
	size_t Size() const {
		if (!mNode || !mNode->is_array()) { return 0; }
		return mNode->size();
	}
	
	std::string GetString() const {
		if (!mNode) { return {}; }
		if (mNode->is_string()) { return mNode->get<std::string>(); }
		// 数値を文字列化など最低限のフォールバック
		if (mNode->is_number()) { return mNode->dump(); }
		return {};
	}
	
	float GetFloat() const {
		if (!mNode) { return 0.f; }
		if (mNode->is_number_float() || mNode->is_number_integer() || mNode->
			is_number_unsigned()) {
			return mNode->get<float>();
		}
		return 0.f;
	}

	[[nodiscard]] int GetInt() const {
		if (!mNode) { return 0; }
		if (mNode->is_number_integer() || mNode->is_number_unsigned()) {
			return mNode->get<int>();
		}
		if (mNode->is_number_float()) {
			return static_cast<int>(mNode->get<float>());
		}
		return 0;
	}

	// 配列ラッパを返す (UWorld::LoadFromJson では GetArray() の戻りに対し [i] / GetFloat を呼ぶ)
	JsonReader GetArray() const { return *this; }
	
	template <typename T>
	std::optional<T> Read(const std::string_view& key) const {
		if (!Has(key)) { return std::nullopt; }
		try { return (*mNode)[std::string(key)].get<T>(); } catch (...) {
			return std::nullopt;
		}
	}
	
	template <typename T>
	std::vector<T> ReadArray(const std::string_view& key) const {
		std::vector<T> out;
		if (!Has(key)) { return out; }
		const auto& j = (*mNode)[std::string(key)];
		if (!j.is_array()) { return out; }
		out.reserve(j.size());
		for (auto& v : j) {
			try { out.emplace_back(v.get<T>()); } catch (...) {
				/* skip */
			}
		}
		return out;
	}

private:
	JsonReader(std::shared_ptr<nlohmann::json> storage,
	           const nlohmann::json*           node, const bool valid)
		: mStorage(std::move(storage)), mNode(node),
		  mValid(valid && node != nullptr) {
	}

	std::shared_ptr<nlohmann::json> mStorage;
	const nlohmann::json*           mNode{nullptr};
	bool                            mValid{false};
};
