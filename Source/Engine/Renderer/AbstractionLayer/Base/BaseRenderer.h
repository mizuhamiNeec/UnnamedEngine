#pragma once
#include "BaseDevice.h"

enum class API {
	DX12,
	Vulkan,
};

struct RendererInitInfo {
	API api;
	void* customAllocator; // ユーザー定義のアロケータ(nullptrの場合はデフォルトのアロケータを使用)
	void* windowHandle; // ウィンドウハンドル
	bool enableDebugLayer; // デバッグレイヤーを有効にするかどうか
	bool enableValidationLayer; // バリデーションレイヤーを有効にするかどうか
};

class BaseRenderer {
public:
	virtual ~BaseRenderer() = default;

	// Rendererの初期化とシャットダウン API固有の処理は実装側で行う
	virtual bool Init(const RendererInitInfo& info) = 0;
	virtual void Shutdown() = 0;

	// GPUデバイスの取得
	virtual BaseDevice* GetDevice() = 0;
};
