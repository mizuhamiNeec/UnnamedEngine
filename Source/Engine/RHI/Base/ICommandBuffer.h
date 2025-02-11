#pragma once
#include <cstdint>

class ICommandBuffer {
public:
	virtual ~ICommandBuffer() = default;

	virtual void Begin() = 0; // バッファ記録の開始
	virtual void End() = 0; // バッファ記録の終了
	virtual void Reset() = 0; // バッファのリセット
	virtual void BeginRenderPass() = 0; // レンダーパスの開始
	virtual void EndRenderPass() = 0; // レンダーパスの終了

	virtual void Render(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0; // 描画
};
