#pragma once

#include "../Renderer/D3D12.h"

#include "../Lib/Structs/Structs.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#else
struct ImVec4 {
	float x, y, z, w;
};
#endif

class SrvManager;

struct ColorTransition {
	ImVec4 targetColor;
	bool isTransitioning;
};

class ImGuiManager {
public:
	void Init(const ::D3D12* renderer, const ::Window* window, const SrvManager* srvManager);
	static void NewFrame();
	void EndFrame() const;
	void Shutdown();

private:
	static void StyleColorsDark(ImGuiStyle* dst = nullptr);

	const D3D12* renderer_ = nullptr;
	const SrvManager* srvManager_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};

#ifdef _DEBUG
void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive);
bool EditTransform(const std::string& name, Transform& transform, const float& vSpeed);
bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed);
void TextOutlined(ImDrawList* drawList, const ImVec2& pos, const char* text, ImU32 textColor, ImU32 outlineColor,
	float outlineSize = 1.0f);
#endif
