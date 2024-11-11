#pragma once
#include <functional>
#include <string>

#include "Console.h"
#include "IConVar.h"

enum class ConVarFlags {
	ConVarFlags_None = 0, // フラグを適用しない
	ConVarFlags_Archive = 1 << 0, // 値がファイルに保存されます
	// ConVarFlags_Cheat = 1 << 1, // TODO : マルチプレイ用
	ConVarFlags_Hidden = 1 << 2, // 隠しコマンド!!
	ConVarFlags_Notify = 1 << 3, // 値が変更された際に通知を表示します
};

std::string ToString(const ConVarFlags& e);

inline ConVarFlags operator|(const ConVarFlags& a, const ConVarFlags& b) {
	return static_cast<ConVarFlags>(static_cast<int>(a) | static_cast<int>(b));
}

bool HasFlags(const ConVarFlags& flags, const ConVarFlags& flag);

template <typename T>
class ConVar : public IConVar {
public:
	ConVar(
		std::string name,
		const T& defaultValue,
		std::string description,
		const ConVarFlags flags,
		const bool& bMin,
		const float& fMin,
		const bool& bMax,
		const float& fMax
	):
		name_(std::move(name)),
		value_(defaultValue),
		defaultValue_(defaultValue),
		helpString_(std::move(description)),
		flags_(flags),
		bMin_(bMin),
		fMin_(fMin),
		bMax_(bMax),
		fMax_(fMax) {
	}

	[[nodiscard]] std::string GetTypeAsString() const override {
		if constexpr (std::is_same_v<T, bool>) {
			return "bool";
		}
		else if constexpr (std::is_same_v<T, int>) {
			return "int";
		}
		else if constexpr (std::is_same_v<T, float>) {
			return "float";
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			return "string";
		}
		else return "unknown";
	}

	[[nodiscard]] std::string GetValueAsString() const override {
		if constexpr (std::is_same_v<T, bool>) {
			return std::to_string(value_);
		}
		else if constexpr (std::is_same_v<T, int>) {
			return std::to_string(value_);
		}
		else if constexpr (std::is_same_v<T, float>) {
			return std::to_string(value_);
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			return value_;
		}
		else
			return "";
	}

	[[nodiscard]] const std::string& GetName() const override {
		return name_;
	}

	[[nodiscard]] const std::string& GetHelp() const override {
		return helpString_;
	}

	void SetValueFromString(const std::string& valueStr) override {
		if constexpr (std::is_same_v<T, bool>) {
			if (value_) {
				value_ = true;
			}
			else {
				value_ = false;
			}
		}
		else if constexpr (std::is_same_v<T, int>) {
			value_ = std::stoi(valueStr);
		}
		else if constexpr (std::is_same_v<T, float>) {
			value_ = std::stof(valueStr);
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			value_ = valueStr;
		}
	}

	[[nodiscard]] ConVarFlags GetFlags() const;

	T GetValue() const;
	void SetValue(const T& newValue);

private:
	std::string name_;
	T value_;
	T defaultValue_;
	std::string helpString_;
	ConVarFlags flags_;
	bool bMin_ = false;
	float fMin_ = 0.0f;
	bool bMax_ = false;
	float fMax_ = 0.0f;
};
