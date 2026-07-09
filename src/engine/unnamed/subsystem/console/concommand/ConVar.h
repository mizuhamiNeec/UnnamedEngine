#pragma once
#include <functional>

#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include "core/math/Vec3.h"

namespace Unnamed {
	/// @brief コンソール変数クラス
	template <typename T>
	class ConVar : public ConCommandBase {
	public:
		using OnChange = std::function<void(const T&)>;

		~ConVar() override;

		ConVar(
			const std::string_view& name,
			const T&                defaultValue,
			const FCVAR&            flags = FCVAR::NONE
		);

		ConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description
		);

		ConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description,
			const bool&             bMin,
			const T&                minValue,
			const bool&             bMax,
			const T&                maxValue
		);

		ConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description,
			const OnChange&         onValueChange
		);

		ConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description,
			const bool&             bMin,
			const T&                minValue,
			const bool&             bMax,
			const T&                maxValue,
			const OnChange&         onValueChange
		);

		// static_cast(objName)で変換できます やったね
		explicit operator T() const {
			return mValue;
		}

		[[nodiscard]] bool IsCommand() const override;

		ConVar& operator=(const T& value);

		[[nodiscard]] T GetValue() const {
			return mValue;
		}

		void SetValue(const T& value) {
			mValue = value;
		}

		[[nodiscard]] T GetDefaultValue() const {
			return mDefaultValue;
		}

		[[nodiscard]] T GetMinValue() const {
			return mMinValue;
		}

		[[nodiscard]] T GetMaxValue() const {
			return mMaxValue;
		}

		[[nodiscard]] bool HasMinValue() const {
			return mHasMinValue;
		}

		[[nodiscard]] bool HasMaxValue() const {
			return mHasMaxValue;
		}

		OnChange onChangeCallback;

	private:
		void RegisterSelf();

		T    mValue;
		T    mDefaultValue;
		T    mMinValue;
		T    mMaxValue;
		bool mHasMinValue = false;
		bool mHasMaxValue = false;
	};

	/// @brief デストラクタ
	template <typename T>
	ConVar<T>::~ConVar() {
		this->UnregisterFromConsoleSystem();
	}

	/// @brief コンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	template <typename T>
	ConVar<T>::ConVar(
		const std::string_view& name, const T& defaultValue,
		const FCVAR&            flags
	) : ConCommandBase(name, "", flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()) {
		RegisterSelf();
	}

	/// @brief コンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	template <typename T>
	ConVar<T>::ConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description
	) : ConCommandBase(name, description, flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()) {
		RegisterSelf();
	}

	/// @brief コンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	/// @param bMin 最小値制限の有無
	/// @param minValue 最小値
	/// @param bMax 最大値制限の有無
	/// @param maxValue 最大値
	template <typename T>
	ConVar<T>::ConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue
	) : ConCommandBase(name, description, flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(minValue),
	    mMaxValue(maxValue),
	    mHasMinValue(bMin),
	    mHasMaxValue(bMax) {
		RegisterSelf();
	}

	/// @brief コンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	/// @param onValueChange 値変更時コールバック
	template <typename T>
	ConVar<T>::ConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const OnChange&         onValueChange
	) : ConCommandBase(name, description, flags),
	    onChangeCallback(onValueChange),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()) {
		RegisterSelf();
	}

	/// @brief コンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	/// @param bMin 最小値制限の有無
	/// @param minValue 最小値
	/// @param bMax 最大値制限の有無
	/// @param maxValue 最大値
	/// @param onValueChange 値変更時コールバック
	template <typename T>
	ConVar<T>::ConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue,
		const OnChange&         onValueChange
	) : ConCommandBase(name, description, flags),
	    onChangeCallback(onValueChange),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(minValue),
	    mMaxValue(maxValue),
	    mHasMinValue(bMin),
	    mHasMaxValue(bMax) {
		RegisterSelf();
	}

	template <typename T>
	bool ConVar<T>::IsCommand() const {
		return false;
	}

	/// @brief 代入演算子のオーバーロード
	/// @param value 代入する値
	/// @return 自身の参照
	template <typename T>
	ConVar<T>& ConVar<T>::operator=(const T& value) {
		if (mValue != value) {
			mValue = value;
			if (onChangeCallback) {
				onChangeCallback(mValue);
			}
		}
		return *this;
	}

	/// @brief 自身をコンソールシステムに登録します
	template <typename T>
	void ConVar<T>::RegisterSelf() {
		const auto console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			Error(
				"ConVar",
				"登録に失敗しました: {}",
				GetName()
			);
			return;
		}
		console->RegisterConVar(this);
	}

	enum class CVAR_TYPE {
		NONE,
		BOOL,
		INT,
		FLOAT,
		DOUBLE,
		STRING,
		VEC3,
	};

	namespace {
		/// @brief CONVAR_TYPEを文字列に変換します
		const char* ToString(const CVAR_TYPE e) {
			switch (e) {
				case CVAR_TYPE::NONE: return "NONE";
				case CVAR_TYPE::BOOL: return "BOOL";
				case CVAR_TYPE::INT: return "INT";
				case CVAR_TYPE::FLOAT: return "FLOAT";
				case CVAR_TYPE::STRING: return "STRING";
				case CVAR_TYPE::VEC3: return "VEC3";
				default: return "unknown";
			}
		}

		/// @brief ConCommandBaseからCONVAR_TYPEを取得します
		CVAR_TYPE GetConVarType(ConCommandBase* var) {
			auto type = CVAR_TYPE::NONE;

			if (dynamic_cast<ConVar<bool>*>(
				var)) {
				type = CVAR_TYPE::BOOL;
			} else if (dynamic_cast<
				ConVar<int>*>(var)) {
				type = CVAR_TYPE::INT;
			} else if (
				dynamic_cast<ConVar<float>*>(
					var)) {
				type = CVAR_TYPE::FLOAT;
			} else if (dynamic_cast<
				ConVar<double>*>(
				var)) {
				type = CVAR_TYPE::DOUBLE;
			} else if (dynamic_cast<
				ConVar<std::string>*>(
				var)) {
				type = CVAR_TYPE::STRING;
			} else if (dynamic_cast<
				ConVar<Vec3>*>(var)) {
				type = CVAR_TYPE::VEC3;
			}

			return type;
		}
	}
}
