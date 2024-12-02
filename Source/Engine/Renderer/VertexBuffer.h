#pragma once

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

class VertexBuffer {
public:
	VertexBuffer(const ComPtr<ID3D12Device>& device, size_t size, size_t stride, const void* pInitData);
	D3D12_VERTEX_BUFFER_VIEW View() const;

	void Update(const void* newVertices, size_t vertexCount);

	size_t GetSize() const { return size_; }

private:
	ComPtr<ID3D12Resource> buffer_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW view_ = {};

	size_t size_ = 0; // バッファサイズを保持する

	VertexBuffer(const VertexBuffer&) = delete;
	void operator=(const VertexBuffer&) = delete;
};
