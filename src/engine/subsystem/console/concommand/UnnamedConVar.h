#pragma once
#include <functional>

#include <engine/subsystem/console/Log.h>
#include <engine/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	/// @brief 名前なしコンソール変数クラス
	template <typename T>
	class UnnamedConVar : public UnnamedConVarBase {
	public:
		using OnChange = std::function<void(const T&)>;

		~UnnamedConVar() override;

		UnnamedConVar(
			const std::string_view& name,
			const T&                defaultValue,
			const FCVAR&            flags = FCVAR::NONE
		);

		UnnamedConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description
		);

		UnnamedConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description,
			const bool&             bMin,
			const T&                minValue,
			const bool&             bMax,
			const T&                maxValue
		);

		UnnamedConVar(
			const std::string_view& name,
			const T&                defaultValue,
			FCVAR                   flags,
			const std::string_view& description,
			const OnChange&         onValueChange
		);

		UnnamedConVar(
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
		explicit operator T() const { return mValue; }

		UnnamedConVar& operator=(const T& value);

		void SetValue(const T& value) {
			mValue = value;
		}

		[[nodiscard]] T GetValue() const {
			return mValue;
		}

		OnChange onChangeCallback;

	private:
		void RegisterSelf();

		T                  mValue;
		T                  mDefaultValue;
		T                  mMinValue;
		T                  mMaxValue;
		bool               mHasMinValue = false;
		bool               mHasMaxValue = false;
		UnnamedConVarBase* mParent      = nullptr; // 登録されている親のConVarBaseポインタ
	};

	/// @brief 名前なしコンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	template <typename T>
	UnnamedConVar<T>::UnnamedConVar(
		const std::string_view& name, const T& defaultValue,
		const FCVAR&            flags
	) : UnnamedConVarBase(name, "", flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()),
	    mHasMinValue(false),
	    mHasMaxValue(false) {
		RegisterSelf();
	}

	/// @brief 名前なしコンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	template <typename T>
	UnnamedConVar<T>::UnnamedConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description
	) : UnnamedConVarBase(name, description, flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()),
	    mHasMinValue(false),
	    mHasMaxValue(false) {
		RegisterSelf();
	}

	/// @brief 名前なしコンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	/// @param bMin 最小値制限の有無
	/// @param minValue 最小値
	/// @param bMax 最大値制限の有無
	/// @param maxValue 最大値
	template <typename T>
	UnnamedConVar<T>::UnnamedConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue
	) : UnnamedConVarBase(name, description, flags),
	    onChangeCallback({}),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(minValue),
	    mMaxValue(maxValue),
	    mHasMinValue(bMin),
	    mHasMaxValue(bMax) {
		RegisterSelf();
	}

	/// @brief 名前なしコンソール変数クラスの実装
	/// @param name 変数名
	/// @param defaultValue デフォルト値
	/// @param flags フラグ
	/// @param description 説明文
	/// @param onValueChange 値変更時コールバック
	template <typename T>
	UnnamedConVar<T>::UnnamedConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const OnChange&         onValueChange
	) : UnnamedConVarBase(name, description, flags),
	    onChangeCallback(onValueChange),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(T()),
	    mMaxValue(T()),
	    mHasMinValue(false),
	    mHasMaxValue(false) {
		RegisterSelf();
	}

	/// @brief 名前なしコンソール変数クラスの実装
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
	UnnamedConVar<T>::UnnamedConVar(
		const std::string_view& name,
		const T&                defaultValue,
		const FCVAR             flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue,
		const OnChange&         onValueChange
	) : UnnamedConVarBase(name, description, flags),
	    onChangeCallback(onValueChange),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mMinValue(minValue),
	    mMaxValue(maxValue),
	    mHasMinValue(bMin),
	    mHasMaxValue(bMax) {
		RegisterSelf();
	}

	/// @brief デストラクタ
	template <typename T>
	UnnamedConVar<T>::~UnnamedConVar() = default;

	/// @brief 代入演算子のオーバーロード
	/// @param value 代入する値
	/// @return 自身の参照
	template <typename T>
	UnnamedConVar<T>& UnnamedConVar<T>::operator=(const T& value) {
		if (mValue != value) {
			mValue = value;
			if (onChangeCallback) {
				mOnChangeCallback(mValue);
			}
		}
		return *this;
	}

	/// @brief 自身をコンソールシステムに登録します
	template <typename T>
	void UnnamedConVar<T>::RegisterSelf() {
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

		DevMsg(
			"ConVar",
			"Name: {}\n"
			"Value: {}\n"
			"DefaultValue: {}\n"
			"MinValue: {}\n"
			"MaxValue: {}\n",
			GetName(),
			mValue,
			mDefaultValue,
			mMinValue,
			mMaxValue
		);
	}

	enum class CONVAR_TYPE {
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
		const char* ToString(const CONVAR_TYPE e) {
			switch (e) {
			case CONVAR_TYPE::NONE: return "NONE";
			case CONVAR_TYPE::BOOL: return "BOOL";
			case CONVAR_TYPE::INT: return "INT";
			case CONVAR_TYPE::FLOAT: return "FLOAT";
			case CONVAR_TYPE::STRING: return "STRING";
			case CONVAR_TYPE::VEC3: return "VEC3";
			default: return "unknown";
			}
		}

		/// @brief UnnamedConVarBaseからCONVAR_TYPEを取得します
		CONVAR_TYPE GetConVarType(UnnamedConVarBase* var) {
			auto type = CONVAR_TYPE::NONE;

			if (dynamic_cast<UnnamedConVar<bool>*>(var)) {
				type = CONVAR_TYPE::BOOL;
			} else if (dynamic_cast<UnnamedConVar<int>*>(var)) {
				type = CONVAR_TYPE::INT;
			} else if (dynamic_cast<UnnamedConVar<float>*>(var)) {
				type = CONVAR_TYPE::FLOAT;
			} else if (dynamic_cast<UnnamedConVar<double>*>(var)) {
				type = CONVAR_TYPE::DOUBLE;
			} else if (dynamic_cast<UnnamedConVar<std::string>*>(var)) {
				type = CONVAR_TYPE::STRING;
			} else if (dynamic_cast<UnnamedConVar<Vec3>*>(var)) {
				type = CONVAR_TYPE::VEC3;
			}

			return type;
		}
	}
}
