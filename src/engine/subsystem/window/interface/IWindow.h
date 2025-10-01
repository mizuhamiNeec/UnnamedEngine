#pragma once
#include <string>

class IWindow {
public:
	struct WindowInfo {
		std::string title           = "Unnamed Window";
		uint32_t    clWidth         = 1280;
		uint32_t    clHeight        = 720;
		bool        bIsInactive     = false;
		bool        bIsResizable    = true;
		bool        bCreateAtCenter = true;
	};

	virtual ~IWindow() = default;

	[[nodiscard]] virtual bool       ShouldClose() const = 0; // 閉じて良いか?
	[[nodiscard]] virtual WindowInfo GetInfo() const = 0; // ウィンドウ情報を取得
	[[nodiscard]] virtual void*      GetNativeHandle() const = 0; // ハンドル
	virtual void                     RequestClose() = 0; // 閉じなさい!シ○ジ君!!

	virtual void SetTitle(std::string_view title) = 0;
	virtual void SetInactive(bool flag) = 0; // プロシージャから使用されます
	virtual void OnResize(uint32_t width, uint32_t height) = 0;
};
