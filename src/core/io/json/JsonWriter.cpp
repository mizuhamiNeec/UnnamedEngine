#include <fstream>
#include "core/filesystem/Path.h"
#include <stdexcept>

#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

#include "JsonWriter.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	void JsonWriter::BeginContainerInternal(nlohmann::json value) {
		// 変更があるのでキャッシュ無効化
		mStringCache.clear();
		auto* cur = Current();
		if (!cur) {
			// ルート未設定の場合のみ許可
			if (!mRoot.is_null()) {
				throw std::runtime_error("Root value already set");
			}
			mRoot = std::move(value);
			mStack.push_back(&mRoot);
			return;
		}

		if (cur->is_object()) {
			if (!mPendingKey.has_value()) {
				throw std::runtime_error(
					"Key() must be called before beginning a container inside an object"
				);
			}
			(*cur)[*mPendingKey] = std::move(value);
			auto& ref            = (*cur)[*mPendingKey];
			mPendingKey.reset();
			mStack.push_back(&ref);
			return;
		}
		if (cur->is_array()) {
			cur->push_back(std::move(value));
			auto& ref = cur->back();
			mStack.push_back(&ref);
			return;
		}
		throw std::runtime_error(
			"Current JSON container is neither object nor array"
		);
	}

	JsonWriter::JsonWriter(const char* path) :
		JsonWriter(Path(path == nullptr ? "" : path)) {
	}

	JsonWriter::JsonWriter(const std::string& path) :
		JsonWriter(Path(path)) {
	}

	JsonWriter::JsonWriter(Path path) : mRoot(nullptr),
	                                    mPath(std::move(path)) {
		mStack.clear();
	}

	void JsonWriter::BeginObject() {
		BeginContainerInternal(nlohmann::json::object());
	}

	void JsonWriter::EndObject() const {
		if (mStack.empty()) {
			throw std::runtime_error("EndObject() called with empty stack");
		}
		const auto* cur = Current();
		if (!cur->is_object()) {
			throw std::runtime_error(
				"Top of stack is not an object in EndObject()"
			);
		}
		if (mPendingKey.has_value()) {
			throw std::runtime_error(
				"Pending key without value before EndObject()"
			);
		}
		mStack.pop_back();
		mStringCache.clear();
	}

	void JsonWriter::BeginArray() {
		BeginContainerInternal(nlohmann::json::array());
	}

	void JsonWriter::EndArray() const {
		if (mStack.empty()) {
			throw std::runtime_error("EndArray() called with empty stack");
		}
		const auto* cur = Current();
		if (!cur->is_array()) {
			throw std::runtime_error(
				"Top of stack is not an array in EndArray()"
			);
		}
		mStack.pop_back();
		mStringCache.clear();
	}

	void JsonWriter::Key(const std::string& key) const {
		const auto* cur = Current();
		if (!cur) {
			throw std::runtime_error("Key() called with no active container");
		}
		if (!cur->is_object()) {
			throw std::runtime_error(
				"Key() called when current container is not an object"
			);
		}
		if (mPendingKey.has_value()) {
			throw std::runtime_error("Previous key still pending a value");
		}
		mPendingKey = key;
	}

	void JsonWriter::WriteVec3(const char* key, const Vec3& value) {
		Key(key);
		BeginArray();
		Write(value.x);
		Write(value.y);
		Write(value.z);
		EndArray();
	}

	void JsonWriter::WriteVec4(const char* key, const Vec4& value) {
		Key(key);
		BeginArray();
		Write(value.x);
		Write(value.y);
		Write(value.z);
		Write(value.w);
		EndArray();
	}

	bool JsonWriter::Save() const {
		if (mPath.IsEmpty()) {
			throw std::runtime_error("Save path is empty");
		}

		const std::filesystem::path filePath = mPath.Native();
		if (filePath.has_parent_path()) {
			std::filesystem::create_directories(filePath.parent_path());
		}

		std::ofstream ofs(filePath, std::ios::binary);
		if (!ofs) {
			throw std::runtime_error(
				"Failed to open file for writing: " + mPath.ToUtf8()
			);
		}
		ofs << ToString();
		Msg(kChannelNone, "{}", ToString());
		ofs.flush();
		return true;
	}

	std::string_view JsonWriter::ToString() const {
		if (mStringCache.empty()) {
			mStringCache = mRoot.dump(4);
		}
		return mStringCache;
	}
}
