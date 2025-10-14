#pragma once

#include <d3d12.h>

#include <engine/urenderer/GraphicsDevice.h>

#include <runtime/assets/core/UAssetID.h>
#include <runtime/core/math/Math.h>
#include <runtime/render/types/RenderTypes.h>

#include "engine/subsystem/interface/ISubsystem.h"

namespace Unnamed {
	struct UMaterialRuntime;

	class RenderResourceManager;
	class ShaderLibrary;
	class RootSignatureCache;
	class UPipelineCache;
	class UWorld;

	struct LastSubmit {
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		uint64_t                            value = 0;
		bool                                valid = false;
	};

	class URenderSubsystem : public ISubsystem {
	public:
		URenderSubsystem(
			GraphicsDevice*        graphicsDevice,
			RenderResourceManager* renderResourceManager,
			ShaderLibrary*         shaderLibrary,
			RootSignatureCache*    rootSignatureCache,
			UPipelineCache*        pipelineCache
		);

		void BeginFrame();
		void RenderWorld(const UWorld& world);
		void EndFrame();

		void SetView(const RenderView& view) {
			mView = view;
		}

		[[nodiscard]] FrameContext GetContext() const { return mContext; }

		// ISubsystem
		bool Init() override;

		[[nodiscard]] const std::string_view GetName() const override {
			return "RenderSubsystem";
		}

	private:
		void Collect(const UWorld& world, const Mat4& parent);
		void DrawItems();

		struct FrameCBData {
			Mat4  view;
			Mat4  proj;
			Mat4  viewProj;
			Vec3  cameraPos;
			float time;
		};

		struct ObjectCBData {
			Mat4 world;
			Mat4 worldInverseTranspose;
		};

		struct TempCB {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			D3D12_GPU_VIRTUAL_ADDRESS              gpu = 0;
		};

		struct TransientBin {
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> keep;
			Microsoft::WRL::ComPtr<ID3D12Fence>                 fence;
			uint64_t                                            fenceValue = 0;
		};

		static constexpr int kFrameInFlight = kFrameBufferCount;
		std::array<TransientBin, kFrameInFlight> mTransient;

		D3D12_GPU_VIRTUAL_ADDRESS UploadCB(
			const void* src, size_t bytes
		) const;

	private:
		GraphicsDevice*        mGraphicsDevice        = nullptr;
		RenderResourceManager* mRenderResourceManager = nullptr;
		ShaderLibrary*         mShaderLibrary         = nullptr;
		RootSignatureCache*    mRootSignatureCache    = nullptr;
		UPipelineCache*        mPipelineCache         = nullptr;
		UAssetManager*         mAssetManager          = nullptr;

		FrameContext mContext;
		RenderView   mView = {};

		/// @brief レンダリングするアイテムの情報
		struct RenderItem {
			Mat4              world;
			MeshHandle        meshHandle;  // 共有メッシュのハンドル
			UMaterialRuntime* material;

			float                depthVS     = 0.0f;
			AssetID              psoId       = 0;
			AssetID              materialKey = 0;
			ID3D12RootSignature* rsPtr       = nullptr;
		};

		std::vector<RenderItem> mItems;

		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBVA = 0;

		LastSubmit mLastSubmit;
	};
}
