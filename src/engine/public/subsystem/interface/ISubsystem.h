#pragma once
#include <string_view>

///@class ISubsystem
///@brief サブシステムインターフェース
class ISubsystem {
public:
	virtual bool                                 Init() = 0;
	virtual void                                 Update(float deltaTime) = 0;
	virtual void                                 Render();
	virtual void                                 Shutdown() = 0;
	[[nodiscard]] virtual const std::string_view GetName() const = 0;
	virtual                                      ~ISubsystem() = default;
};
