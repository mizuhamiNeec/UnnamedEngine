#pragma once
#include <cstdint>
#include <d3d12.h>

#include <wrl/client.h>

namespace Unnamed ::Rhi {
	/// @brief D3D12FrameUploadAllocatorは、フレームごとにリセットするUpload Heapから整列済み領域を払い出します
	class D3D12FrameUploadAllocator {
	public:
		/// @brief フレームごとにリセットされるアップロード用バッファアロケータ
		/// @param device デバイス
		/// @param bytes バッファのサイズ（256バイト境界にアラインメントされる）
		void Initialize(ID3D12Device* device, uint32_t bytes);

		/// @brief アロケータをシャットダウンします。リソースが解放されます。
		void Shutdown();

		/// @brief フレームの開始時に呼び出されます。内部オフセットがリセットされます。
		void BeginFrame();

		/// @brief 定数バッファ用の領域をアロケートし、データをコピーします。
		/// @param srcData コピー元のデータ
		/// @param bytes コピーするバイト数（256バイト境界にアラインメントされる）
		/// @return GPU仮想アドレス。失敗した場合は0。
		D3D12_GPU_VIRTUAL_ADDRESS AllocateConstantBuffer(
			const void* srcData, uint32_t bytes
		);

	private:
		/// @brief 256バイト境界にアラインメントする
		/// @param v 元の値
		 /// @return アラインメントされた値
		static uint32_t Align256(uint32_t v);

		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		uint8_t*                               mMapped   = nullptr;
		uint32_t                               mCapacity = 0;
		uint32_t                               mOffset   = 0;
	};
}
