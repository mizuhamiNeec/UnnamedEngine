#pragma once
#include <json.hpp>

/// @class JsonReader
/// @brief JSONを読み込むためのクラス
class JsonReader final {
	const nlohmann::json& mRoot;

public:
	explicit JsonReader(const nlohmann::json& root);

	template <typename T>
	std::optional<T> Read(const std::string_view& key) const;

	[[nodiscard]] JsonReader GetChild(const std::string_view& key) const;

	template <typename T>
	std::vector<T> ReadArray(const std::string_view& key) const;
};
