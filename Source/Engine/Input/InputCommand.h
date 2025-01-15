#pragma once
#include <functional>

class InputCommand {
public:
	using Callback = std::function<void()>;

	InputCommand(Callback onPress, Callback onRelease) : onPress_(std::move(onPress)),
	                                                     onRelease_(std::move(onRelease)) {
	}

	void Press() const {
		if (onPress_)
		{
			onPress_();
		}
	}

	void Release() const {
		if (onRelease_)
		{
			onRelease_();
		}
	}

private:
	Callback onPress_;
	Callback onRelease_;
};
