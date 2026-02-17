#pragma once
#include <format>
#include <sstream>
#include <string>
#include <mutex>

#include "Console.h"
#include "IConVar.h"

enum class ConVarFlags {
	ConVarFlags_None    = 0,      // フラグを適用しない
	ConVarFlags_Archive = 1 << 0, // 値がファイルに保存されます
	// ConVarFlags_Cheat = 1 << 1,
	ConVarFlags_Hidden = 1 << 2, // 隠しコマンド!!
	ConVarFlags_Notify = 1 << 3, // 値が変更された際に通知を表示します
};

std::string ToString(const ConVarFlags& e);

inline ConVarFlags operator|(const ConVarFlags& a, const ConVarFlags& b) {
	return static_cast<ConVarFlags>(static_cast<int>(a) | static_cast<int>(b));
}

bool HasFlags(const ConVarFlags& flags, const ConVarFlags& flag);

/// @brief コンソール変数クラス
template <typename T>
class ConVar : public IConVar {
public:
	/// @brief コンストラクタ
	ConVar(
		std::string       name,
		const T&          defaultValue,
		std::string       description,
		const ConVarFlags flags,
		const bool        bMin,
		const float       fMin,
		const bool        bMax,
		const float       fMax
	) :
		mName(std::move(name)),
		mValue(defaultValue),
		mDefaultValue(defaultValue),
		mHelpString(std::move(description)),
		mFlags(flags),
		mBMin(bMin),
		mFMin(fMin),
		mBMax(bMax),
		mFMax(fMax) {}

	/// @brief 型を文字列として取得します
	/// @return 型の文字列表現
	[[nodiscard]] std::string GetTypeAsString() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, bool>) return "bool";
		else if constexpr (std::is_same_v<T, int>) return "int";
		else if constexpr (std::is_same_v<T, float>) return "float";
		else if constexpr (std::is_same_v<T, std::string>) return "string";
		else if constexpr (std::is_same_v<T, Vec3>) return "vec3";
		else return "unknown";
	}

	/// @brief 値を文字列として取得します
	/// @return 文字列としての値
	[[nodiscard]] std::string GetValueAsString() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, std::string>) { return mValue; } else if
		constexpr (std::is_same_v<T, Vec3>) {
			return std::format("{} {} {}", mValue.x, mValue.y, mValue.z);
		} else { return std::to_string(mValue); }
	}

	/// @brief コンソール変数の名前を取得します
	/// @return コンソール変数の名前
	[[nodiscard]] const std::string& GetName() const override { return mName; }

	/// @brief コンソール変数のヘルプテキストを取得します
	/// @return ヘルプテキスト
	[[nodiscard]] const std::string& GetHelp() const override {
		return mHelpString;
	}

	/// @brief 値をfloat型として取得します
	/// @return float型の値
	[[nodiscard]] float GetValueAsFloat() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, float>) { return mValue; } else if
		constexpr (std::is_arithmetic_v<T>) {
			return static_cast<float>(mValue);
		} else {
			throw std::runtime_error(
				"Unsupported type for conversion to float"
			);
		}
	}

	/// @brief 値をdouble型として取得します
	/// @return double型の値
	[[nodiscard]] double GetValueAsDouble() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, double>) { return mValue; } else if
		constexpr (std::is_arithmetic_v<T>) {
			return static_cast<double>(mValue);
		} else {
			throw std::runtime_error(
				"Unsupported type for conversion to double"
			);
		}
	}

	/// @brief 値をint型として取得します
	/// @return int型の値
	[[nodiscard]] int GetValueAsInt() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, int>) { return mValue; } else if
		constexpr (std::is_arithmetic_v<
			T>) { return static_cast<int>(mValue); } else {
			throw std::runtime_error("Unsupported type for conversion to int");
		}
	}

	/// @brief 値をbool型として取得します
	/// @return bool型の値
	[[nodiscard]] bool GetValueAsBool() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, bool>) { return mValue; } else if
		constexpr (std::is_arithmetic_v<T>) { return mValue != 0; } else {
			throw std::runtime_error("Unsupported type for conversion to bool");
		}
	}

	/// @brief 値をVec3型として取得します
	/// @return Vec3型の値
	[[nodiscard]] Vec3 GetValueAsVec3() const override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, Vec3>) { return mValue; } else {
			throw std::runtime_error("Unsupported type for conversion to Vec3");
		}
	}

	/// @brief 文字列から値を設定します
	/// @param valueStr 設定する値の文字列
	void SetValueFromString(const std::string& valueStr) override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<
			T, bool>) {
			SetValue(valueStr == "true" || valueStr == "1");
		} else if constexpr (std::is_same_v<
			T, int>) { SetValue(std::stoi(valueStr)); } else if constexpr (
			std::is_same_v<T, float>) { SetValue(std::stof(valueStr)); } else if
		constexpr (std::is_same_v<
			T, std::string>) { SetValue(valueStr); } else if constexpr (
			std::is_same_v<T, Vec3>) {
			float x, y, z;
			try {
				// スペースで区切られた3つの数値を取得
				std::istringstream iss(valueStr);
				std::string        xStr, yStr, zStr;

				if (iss >> xStr >> yStr >> zStr) {
					x = std::stof(xStr);
					y = std::stof(yStr);
					z = std::stof(zStr);
					SetValue(Vec3(x, y, z));
				} else {
					throw std::runtime_error(
						"Invalid format for Vec3. Expected: x y z"
					);
				}
			} catch (const std::exception&) {
				throw std::runtime_error(
					"Invalid format for Vec3. Expected: x y z"
				);
			}
		}
	}

	/// @brief float型の値から設定します
	/// @param newValue 設定するfloat型の値
	void SetValueFromFloat(const float newValue) override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_convertible_v<float, T>) {
			SetValue(static_cast<T>(newValue));
		} else { PrintConvertErrorMessage(); }
	}

	/// @brief double型の値から設定します
	/// @param newValue 設定するdouble型の値
	void SetValueFromDouble(const double newValue) override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_convertible_v<double, T>) {
			if constexpr (std::is_same_v<T, float>) {
				SetValue(static_cast<T>(static_cast<float>(newValue)));
			} else if constexpr (std::is_same_v<T, Vec3>) {
				SetValue(Vec3(Vec3::one * static_cast<float>(newValue)));
			} else { SetValue(static_cast<T>(newValue)); }
		} else { PrintConvertErrorMessage(); }
	}

	/// @brief int型の値から設定します
	void SetValueFromInt(const int newValue) override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_convertible_v<int, T>) {
			if constexpr (std::is_same_v<T, float>) {
				SetValue(static_cast<T>(static_cast<float>(newValue)));
			} else if constexpr (std::is_same_v<T, std::string>) {
				SetValue(std::to_string(newValue));
			} else if constexpr (std::is_same_v<T, Vec3>) {
				SetValue(Vec3(Vec3::one * static_cast<float>(newValue)));
			} else { SetValue(static_cast<T>(newValue)); }
		} else { PrintConvertErrorMessage(); }
	}

	/// @brief bool型の値から設定します
	void SetValueFromBool(const bool newValue) override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, bool>) { SetValue(newValue); } else {
			PrintConvertErrorMessage();
		}
	}

	/// @brief Vec3型の値から設定します
	[[nodiscard]] ConVarFlags GetFlags() const { return mFlags; }

	/// @brief 値を取得します
	T GetValue() const {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		return mValue;
	}


	/// @brief 値を設定します
	/// @param newValue 設定する新しい値
	void SetValue(const T& newValue) {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		mValue = newValue;

		if (HasFlags(mFlags, ConVarFlags::ConVarFlags_Notify)) {
			// プレイヤーに通知する
			Console::Print(
				std::format(
					"{}のCVAR 値 '{}' を {} に変更しました\n",
					"クライアント",
					mName,
					GetValueAsString()
				),
				kConTextColorWarning,
				Channel::General
			);
		}
	}

	/// @brief 値をトグル（切り替え）します
	void Toggle() override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
		if constexpr (std::is_same_v<T, bool>) { SetValue(!mValue); } else if
		constexpr (std::is_same_v<T, int>) {
			if (mValue == 0) { mValue = 1; } else { mValue = 0; }
		} else {
			Console::Print(
				std::format("{} : CVAR は bool 型か int 型でなければなりません\n", mName),
				kConTextColorError,
				Channel::Console
			);
		}
	}

	/// @brief ImGuiでCVarを描画します
	void DrawImGui() override {
		std::lock_guard<std::recursive_mutex> lock(mMutex);
#ifdef _DEBUG
		if constexpr (std::is_same_v<
			T, bool>) { ImGui::Checkbox(mName.c_str(), &mValue); } else if
		constexpr (std::is_same_v<
			T, int>) { ImGui::DragInt(mName.c_str(), &mValue); } else if
		constexpr (std::is_same_v<
			T, float>) { ImGui::DragFloat(mName.c_str(), &mValue); } else if
		constexpr (std::is_same_v<T, Vec3>) {
			ImGui::DragFloat3(mName.c_str(), &mValue.x);
		} else if constexpr (std::is_same_v<T, std::string>) {
			char buffer[256];
			strncpy_s(buffer, mValue.c_str(), sizeof(buffer));
			if (ImGui::InputText(mName.c_str(), buffer, sizeof(buffer))) {
				mValue = buffer;
			}
		} else {
			//ImGui::InputText(name_.c_str(), &value_);
		}
#endif
	}

private:
	/// @brief 型変換エラーメッセージを表示します
	void PrintConvertErrorMessage() {
		Console::Print(
			std::format(
				"{} : CVAR を {} 型へ変換できませんでした\n", mName,
				GetTypeAsString()
			),
			kConTextColorError,
			Channel::General
		);
	}

	std::string mName;
	T           mValue;
	T           mDefaultValue;
	std::string mHelpString;
	ConVarFlags mFlags;
	bool        mBMin = false;
	float       mFMin = 0.0f;
	bool        mBMax = false;
	float       mFMax = 0.0f;
	mutable std::recursive_mutex mMutex;
};
