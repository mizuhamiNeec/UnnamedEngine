#pragma once
#include <string_view>

///@class ISubsystem
///@brief サブシステムのインターフェース
class ISubsystem {
public:
	// 初期化処理
	virtual bool Init() = 0;

	// 更新処理
	virtual void Update(float) {
	}

	// 描画処理
	virtual void Render() {
	}

	// 終了処理
	virtual void Shutdown() {
	}

	// システム名の取得
	[[nodiscard]] virtual const std::string_view GetName() const = 0;
	virtual                                      ~ISubsystem() = default;
};
