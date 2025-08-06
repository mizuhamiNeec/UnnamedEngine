#pragma once
#include <json.hpp>

/// @class JsonWriter
/// @brief JSONを書き込むためのクラス
class JsonWriter final {
	nlohmann::json mRoot;

public:
	JsonWriter();

	template <typename T>
	void Write(const std::string_view& key, const T& value);

	void Write(const std::string_view& key, const nlohmann::json& value);

	nlohmann::json& GetJson();

	[[nodiscard]] std::string ToString() const;
};
