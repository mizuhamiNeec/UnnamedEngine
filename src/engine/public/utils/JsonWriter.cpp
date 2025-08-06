#include <engine/public/utils/JsonWriter.h>

JsonWriter::JsonWriter() = default;

template <typename T>
void JsonWriter::Write(const std::string_view& key, const T& value) {
	mRoot[key] = value;
}

void JsonWriter::Write(const std::string_view& key,
	const nlohmann::json& value) {
	mRoot[key] = value;
}

nlohmann::json& JsonWriter::GetJson() { return mRoot; }

std::string JsonWriter::ToString() const { return mRoot.dump(); }
