#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/urenderer/GraphicsDevice.h>

#include "core/json/JsonReader.h"

#include "engine/VertexFormats.h"

#include "runtime/assets/core/UAssetManager.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "MeshRendererComponent";

	/// @brief メッシュレンダラーコンポーネントがアタッチされたときの処理
	void MeshRendererComponent::OnAttached() {
		BaseComponent::OnAttached();
	}

	/// @brief メッシュレンダラーコンポーネントがデタッチされたときの処理
	void MeshRendererComponent::OnDetached() {
		BaseComponent::OnDetached();
	}

	/// @brief シリアライズ
	/// @param writer JSONライター
	void MeshRendererComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	/// @brief デシリアライズ
	/// @param reader JSONリーダー
	void MeshRendererComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("mesh")) {
			// TODO: AssetManagerからロード
		}
		if (reader.Has("material")) {
			// 文字列をAssetに解決して materialAsset
		}
	}

	///	 @brief GPUリソースの確保
	/// @param graphicsDevice グラフィックスデバイス
	/// @param renderResourceManager レンダーリソースマネージャー
	/// @param shaderLibrary シェーダーライブラリ
	/// @param rootSignatureCache ルートシグネチャキャッシュ
	/// @param pipelineCache パイプラインキャッシュ
	/// @param cmd コマンドリスト
	/// @return GPUリソースが確保できたらtrue
	bool MeshRendererComponent::EnsureGPU(
		GraphicsDevice*            graphicsDevice,
		RenderResourceManager*     renderResourceManager,
		ShaderLibrary*             shaderLibrary,
		RootSignatureCache*        rootSignatureCache,
		UPipelineCache*            pipelineCache,
		ID3D12GraphicsCommandList* cmd
	) {
		if (mGPUReady) { return true; }

		if (
			material.materialAsset == kInvalidAssetID &&
			materialAsset != kInvalidAssetID
		) {
			material.materialAsset = materialAsset;
		}

		if (
			!material.BuildCPU(
				renderResourceManager->GetAssetManager(), shaderLibrary,
				rootSignatureCache, pipelineCache, graphicsDevice
			)
		) {
			return false;
		}

		if (!material.IsGPUReady()) {
			material.RealizeGPU(renderResourceManager, cmd);
		}

		// メッシュの取得（共有機構を使用）
		if (!meshHandle.IsValid() && meshAsset != kInvalidAssetID) {
			meshHandle = renderResourceManager->AcquireMesh(meshAsset);
			if (!meshHandle.IsValid()) {
				Error(
					kChannel,
					"Failed to acquire mesh: {}",
					renderResourceManager->GetAssetManager()->Meta(meshAsset).
					name.c_str()
				);
				return false;
			}
		}

		mGPUReady = meshHandle.IsValid();

		return mGPUReady;
	}

	///	@brief GPUリソースの解放
	/// @param renderResourceManager レンダーリソースマネージャー
	void MeshRendererComponent::InvalidateGPU(
		RenderResourceManager* renderResourceManager
	) {
		material.InvalidateGPU(renderResourceManager, nullptr, 0);

		// メッシュの解放
		if (meshHandle.IsValid()) {
			renderResourceManager->ReleaseMesh(meshHandle, nullptr, 0);
			meshHandle = {};
		}

		mGPUReady = false;
	}
}
