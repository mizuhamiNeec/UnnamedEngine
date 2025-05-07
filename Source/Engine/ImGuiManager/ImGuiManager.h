#pragma once

#include "../Renderer/D3D12.h"

#include "../Lib/Structs/Structs.h"

#ifdef _DEBUG
#include <imgui.h>
#else
#endif

class TransformComponent;
class ShaderResourceViewManager;

class ImGuiManager {
public:
	ImGuiManager(const D3D12* renderer, const ShaderResourceViewManager* srvManager);
	static void NewFrame();
	void EndFrame() const;
	void Shutdown();
	void Recreate() const;

#ifdef _DEBUG
	static void StyleColorsDark();
	static void StyleColorsLight();

	static void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive);
	static bool EditTransform(Transform& transform, const float& vSpeed);
	static bool EditTransform(TransformComponent& transform, const float& vSpeed);
	static bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed, const char* format);
	static void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text, ImVec4 textColor, ImVec4 outlineColor,
		float outlineSize = 1.0f
	);
	static bool IconButton(const char* icon, const char* label, const ImVec2& size = ImVec2(0, 0));
#endif

private:
	const D3D12* renderer_ = nullptr;
	const ShaderResourceViewManager* srvManager_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
