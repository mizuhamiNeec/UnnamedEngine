#pragma once
#include <Window/Base/BaseWindow.h>

class EditorWindow final : public BaseWindow {
public:
	EditorWindow();
	~EditorWindow() override;
	bool Create(WindowInfo info) override;
	bool ProcessMessage() override;
	void SetResizeCallback(ResizeCallback callback) override;

protected:
	LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
};
