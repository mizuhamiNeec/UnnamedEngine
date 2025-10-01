#pragma once
#include <functional>

#include <engine/public/subsystem/console/Log.h>
#include <engine/public/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>

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

	private:
		void RegisterSelf();

	private:
		T                  mValue;
		T                  mDefaultValue;
		OnChange           mOnChangeCallback;
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
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mOnChangeCallback({}),
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
		FCVAR                   flags,
		const std::string_view& description
	) : UnnamedConVarBase(name, description, flags),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mOnChangeCallback({}),
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
		FCVAR                   flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue
	) : UnnamedConVarBase(name, description, flags),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mOnChangeCallback({}),
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
		FCVAR                   flags,
		const std::string_view& description,
		const OnChange&         onValueChange
	) : UnnamedConVarBase(name, description, flags),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mOnChangeCallback(onValueChange),
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
		FCVAR                   flags,
		const std::string_view& description,
		const bool&             bMin, const T& minValue,
		const bool&             bMax, const T& maxValue,
		const OnChange&         onValueChange
	) : UnnamedConVarBase(name, description, flags),
	    mValue(defaultValue),
	    mDefaultValue(defaultValue),
	    mOnChangeCallback(onValueChange),
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
			if (mOnChangeCallback) {
				mOnChangeCallback(mValue);
			}
		}
		return *this;
	}

	template <typename T>
	void UnnamedConVar<T>::RegisterSelf() {
		auto console = ServiceLocator::Get<ConsoleSystem>();
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
}
