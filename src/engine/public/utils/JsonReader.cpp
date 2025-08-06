#include <engine/public/utils/JsonReader.h>

JsonReader::JsonReader(const nlohmann::json& root): mRoot(root) {
}

template <typename T>
std::optional<T> JsonReader::Read(const std::string_view& key) const {
	if (mRoot.contains(key)) {
		return mRoot[key].at(key).get<T>();
	}
	return std::nullopt;
}

JsonReader JsonReader::GetChild(const std::string_view& key) const {
	return JsonReader(mRoot.at(key));
}

template <typename T>
std::vector<T> JsonReader::ReadArray(const std::string_view& key) const {
	std::vector<T> array;
	if (mRoot.contains(key)) {
		for (const auto& item : mRoot.at(key)) {
			array.emplace_back(item.get<T>());
		}
	}
	return array;
}
