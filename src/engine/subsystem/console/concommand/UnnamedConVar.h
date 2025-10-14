#pragma once
#include <functional>

#include <engine/subsystem/console/Log.h>
#include <engine/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
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

		T GetValue() const {
			return mValue;
		}

		OnChange onChangeCallback;

	private:
		void RegisterSelf();

	private:
		T                  mValue;
		T                  mDefaultValue;
		T                  mMinValue;
		T                  mMaxValue;
		bool               mHasMinValue = false;
		bool               mHasMaxValue = false;
		UnnamedConVarBase* mParent      = nullptr; // 登録されている親のConVarBaseポインタ
	};

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

	template <typename T>
	UnnamedConVar<T>::~UnnamedConVar() = default;

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
