#pragma once
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
	) :
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
		if constexpr (std::is_same_v<T, bool>)
			return "bool";
		else if constexpr (std::is_same_v<T, int>)
			return "int";
		else if constexpr (std::is_same_v<T, float>)
			return "float";
		else if constexpr (std::is_same_v<T, std::string>)
			return "string";
		else if constexpr (std::is_same_v<T, Vec3>)
			return"vec3";
		else
			return "unknown";
	}

	[[nodiscard]] std::string GetValueAsString() const override {
		if constexpr (std::is_same_v<T, std::string>) {
			return value_;
		} else if constexpr (std::is_same_v<T, Vec3>) {
			return std::format("{} {} {}", value_.x, value_.y, value_.z);
		} else {
			return std::to_string(value_);
		}
	}

	[[nodiscard]] const std::string& GetName() const override {
		return name_;
	}

	[[nodiscard]] const std::string& GetHelp() const override {
		return helpString_;
	}

	[[nodiscard]] float GetValueAsFloat() const override {
		if constexpr (std::is_same_v<T, float>) {
			return value_;
		} else if constexpr (std::is_arithmetic_v<T>) {
			return static_cast<float>(value_);
		} else {
			throw std::runtime_error("Unsupported type for conversion to float");
		}
	}

	[[nodiscard]] double GetValueAsDouble() const override {
		if constexpr (std::is_same_v<T, double>) {
			return value_;
		} else if constexpr (std::is_arithmetic_v<T>) {
			return static_cast<double>(value_);
		} else {
			throw std::runtime_error("Unsupported type for conversion to double");
		}
	}

	[[nodiscard]] int GetValueAsInt() const override {
		if constexpr (std::is_same_v<T, int>) {
			return value_;
		} else if constexpr (std::is_arithmetic_v<T>) {
			return static_cast<int>(value_);
		} else {
			throw std::runtime_error("Unsupported type for conversion to int");
		}
	}

	[[nodiscard]] bool GetValueAsBool() const override {
		if constexpr (std::is_same_v<T, bool>) {
			return value_;
		} else if constexpr (std::is_arithmetic_v<T>) {
			return value_ != 0;
		} else {
			throw std::runtime_error("Unsupported type for conversion to bool");
		}
	}

	[[nodiscard]] Vec3 GetValueAsVec3() const override {
		if constexpr (std::is_same_v<T, Vec3>) {
			return value_;
		} else {
			throw std::runtime_error("Unsupported type for conversion to Vec3");
		}
	}

	void SetValueFromString(const std::string& valueStr) override {
		if constexpr (std::is_same_v<T, bool>) {
			SetValue(valueStr == "true" || valueStr == "1");
		} else if constexpr (std::is_same_v<T, int>) {
			SetValue(std::stoi(valueStr));
		} else if constexpr (std::is_same_v<T, float>) {
			SetValue(std::stof(valueStr));
		} else if constexpr (std::is_same_v<T, std::string>) {
			SetValue(valueStr);
		} else if constexpr (std::is_same_v<T, Vec3>) {
			float x, y, z;
			try {
				// スペースで区切られた3つの数値を取得
				std::istringstream iss(valueStr);
				std::string xStr, yStr, zStr;

				if (iss >> xStr >> yStr >> zStr) {
					x = std::stof(xStr);
					y = std::stof(yStr);
					z = std::stof(zStr);
					SetValue(Vec3(x, y, z));
				} else {
					throw std::runtime_error("Invalid format for Vec3. Expected: x y z");
				}
			} catch (const std::exception&) {
				throw std::runtime_error("Invalid format for Vec3. Expected: x y z");
			}
		}
	}

	void SetValueFromFloat(const float& newValue) override {
		if constexpr (std::is_convertible_v<float, T>) {
			SetValue(static_cast<T>(newValue));
		} else {
			PrintConvertErrorMessage();
		}
	}

	void SetValueFromDouble(const double& newValue) override {
		if constexpr (std::is_convertible_v<double, T>) {
			if constexpr (std::is_same_v<T, float>) {
				SetValue(static_cast<T>(static_cast<float>(newValue)));
			} else if constexpr (std::is_same_v<T, Vec3>) {
				SetValue(Vec3(Vec3::one * static_cast<float>(newValue)));
			} else {
				SetValue(static_cast<T>(newValue));
			}
		} else {
			PrintConvertErrorMessage();
		}
	}

	void SetValueFromInt(const int& newValue) override {
		if constexpr (std::is_convertible_v<int, T>) {
			if constexpr (std::is_same_v<T, float>) {
				SetValue(static_cast<T>(static_cast<float>(newValue)));
			} else if constexpr (std::is_same_v<T, std::string>) {
				SetValue(std::to_string(newValue));
			} else if constexpr (std::is_same_v<T, Vec3>) {
				SetValue(Vec3(Vec3::one * static_cast<float>(newValue)));
			} else {
				SetValue(static_cast<T>(newValue));
			}
		} else {
			PrintConvertErrorMessage();
		}
	}

	void SetValueFromBool(const bool& newValue) override {
		if constexpr (std::is_same_v<T, bool>) {
			SetValue(newValue);
		} else {
			PrintConvertErrorMessage();
		}
	}

	[[nodiscard]] ConVarFlags GetFlags() const {
		return flags_;
	}

	T GetValue() const {
		return value_;
	}

	void SetValue(const T& newValue) {
		value_ = newValue;

		if (HasFlags(flags_, ConVarFlags::ConVarFlags_Notify)) {
			// プレイヤーに通知する
			Console::Print(
				std::format(
					"{}のCVAR 値 '{}' を {} に変更しました\n",
					"クライアント",
					name_,
					GetValueAsString()
				),
				kConTextColorWarning,
				Channel::General
			);
		}
	}

	void Toggle() override {
		if constexpr (std::is_same_v<T, bool>) {
			SetValue(!value_);
		} else if constexpr (std::is_same_v<T, int>) {
			if (value_ == 0) {
				value_ = 1;
			} else {
				value_ = 0;
			}
		} else {
			Console::Print(
				std::format("{} : CVAR は bool 型か int 型でなければなりません\n", name_),
				kConTextColorError,
				Channel::Console
			);
		}
	}

	void DrawImGui() override {
#ifdef _DEBUG
		if constexpr (std::is_same_v<T, bool>) {
			ImGui::Checkbox(name_.c_str(), &value_);
		} else if constexpr (std::is_same_v<T, int>) {
			ImGui::DragInt(name_.c_str(), &value_);
		} else if constexpr (std::is_same_v<T, float>) {
			ImGui::DragFloat(name_.c_str(), &value_);
		} else if constexpr (std::is_same_v<T, Vec3>) {
			ImGui::DragFloat3(name_.c_str(), &value_.x);
		} else if constexpr (std::is_same_v<T, std::string>) {
			char buffer[256];
			strncpy_s(buffer, value_.c_str(), sizeof(buffer));
			if (ImGui::InputText(name_.c_str(), buffer, sizeof(buffer))) {
				value_ = buffer;
			}
		} else {
			ImGui::InputText(name_.c_str(), &value_);
		}
#endif
	}

private:
	void PrintConvertErrorMessage() {
		Console::Print(
			std::format("{} : CVAR を {} 型へ変換できませんでした\n", name_, GetTypeAsString()),
			kConTextColorError,
			Channel::General
		);
	}

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
