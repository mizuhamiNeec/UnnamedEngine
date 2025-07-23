#pragma once

#include <list>
#include <memory>
#include <unordered_map>

#include <engine/public/renderer/ConstantBuffer.h>
#include <engine/public/renderer/IndexBuffer.h>
#include <engine/public/renderer/PipelineState.h>
#include <engine/public/renderer/RootSignatureManager.h>
#include <engine/public/renderer/Structs.h>
#include <engine/public/renderer/VertexBuffer.h>


class SrvManager;
class CameraComponent;
class Camera;
class D3D12;

enum class ParticleMeshType {
	Quad,
	Ring,
};

struct MeshData {
	std::vector<Vertex>                   vertices;
	std::vector<uint32_t>                 indices;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer;
	std::unique_ptr<IndexBuffer>          indexBuffer;
};

class ParticleManager {
public:
	void Init(D3D12* d3d12, SrvManager* srvManager);
	void Shutdown();

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Update(float deltaTime);
	void Render();

	static std::vector<Vertex> GenerateRingVertices(
		float innerRadius, float outerRadius, int segments);
	static std::vector<uint32_t> GenerateRingIndices(int segments);

	void RegisterMesh(ParticleMeshType meshType, std::vector<Vertex>& vertices,
	                  const std::vector<uint32_t>& indices);

	MeshData& GetMeshData(ParticleMeshType type);

	void Emit(const std::string& name, const Vec3& pos, const uint32_t& count);

	[[nodiscard]] D3D12* GetD3D12() const;

	// Getter
	[[nodiscard]] CameraComponent* GetDefaultCamera() const;
	[[nodiscard]] SrvManager*      GetSrvManager() const;
	const VertexBuffer<Vertex>*    GetVertexBuffer();
	const IndexBuffer*             GetIndexBuffer();
	const std::vector<Vertex>&     GetVertices();
	const std::vector<uint32_t>&   GetIndices();

	void CreateParticleGroup(const std::string& name,
	                         const std::string& textureFilePath);

private:
	struct ParticleGroup {
		MaterialData                    materialData; // マテリアルデータ
		std::list<Particle>             particles; // パーティクルのリスト
		uint32_t                        srvIndex = 0; // インスタンシングデータ用SRVインデックス
		std::unique_ptr<ConstantBuffer> instancingResource = nullptr;
		// インスタンシングリソース
		uint32_t         numInstance    = 0; // インスタンス数
		ParticleForGPU*  instancingData = nullptr; // インスタンシングデータを書き込むためのポインタ
		ParticleMeshType meshType       = ParticleMeshType::Quad; // メッシュの種類
	};

	// ユーザがつけるグループ名をキーとして、グループを複数持てるようにする
	std::unordered_map<std::string, ParticleGroup> mParticleGroups;

	std::vector<std::string> mRegisteredGroupNames;

	D3D12*                                mRenderer             = nullptr;
	std::unique_ptr<RootSignatureManager> mRootSignatureManager = nullptr;
	SrvManager*                           mSrvManager           = nullptr;
	std::unique_ptr<PipelineState>        mPipelineState        = nullptr;
	CameraComponent*                      mDefaultCamera        = nullptr;

	uint32_t mKNumMaxInstance = 16385; // 最大インスタンス数

	std::unordered_map<ParticleMeshType, MeshData> mEshData;
};
