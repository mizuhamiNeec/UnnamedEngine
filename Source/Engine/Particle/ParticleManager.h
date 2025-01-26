#pragma once

#include <list>
#include <memory>
#include <unordered_map>

#include <Lib/Structs/Structs.h>

#include <Renderer/ConstantBuffer.h>
#include <Renderer/PipelineState.h>
#include <Renderer/RootSignatureManager.h>
#include <Renderer/VertexBuffer.h>

class CameraComponent;
class Camera;
class D3D12;

class ParticleManager {
public:
	//void Init(D3D12* d3d12, SrvManager* srvManager);
	void Shutdown() const;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Update(float deltaTime);
	void Render();

	void Emit(const std::string& name, const Vec3& pos, const uint32_t& count);

	D3D12* GetD3D12() const;

	// Getter
	CameraComponent* GetDefaultCamera() const;
	// SrvManager* GetSrvManager() const;

	const VertexBuffer<Vertex>* GetVertexBuffer() const;
	const std::vector<Vertex>& GetVertices() const;

	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

private:
	struct ParticleGroup {
		MaterialData materialData; // マテリアルデータ
		std::list<Particle> particles; // パーティクルのリスト
		uint32_t srvIndex = 0; // インスタンシングデータ用SRVインデックス
		std::unique_ptr<ConstantBuffer> instancingResource = nullptr; // インスタンシングリソース
		uint32_t numInstance = 0; // インスタンス数
		ParticleForGPU* instancingData = nullptr; // インスタンシングデータを書き込むためのポインタ
	};

	// ユーザがつけるグループ名をキーとして、グループを複数持てるようにする
	std::unordered_map<std::string, ParticleGroup> particleGroups_;

	D3D12* d3d12_ = nullptr;
	CameraComponent* defaultCamera_ = nullptr;
	std::unique_ptr<RootSignatureManager> rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
	//SrvManager* srvManager_ = nullptr;

	uint32_t kNumMaxInstance = 512;

	std::vector<Vertex> vertices_;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_;
};
