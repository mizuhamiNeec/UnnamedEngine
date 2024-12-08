#pragma once

#include <list>
#include <memory>

#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Structs/Structs.h"

#include "../Renderer/ConstantBuffer.h"
#include "../Renderer/IndexBuffer.h"
#include "../Renderer/VertexBuffer.h"

class SrvManager;
class Camera;
class ParticleCommon;

class ParticleObject {
public:
	~ParticleObject() = default;

	void Init(ParticleCommon* particleCommon, const std::string& textureFilePath);
	void Update(float deltaTime);
	void Draw() const;

	static Particle MakeNewParticle(const Vec3& pos);

	static std::list<Particle> Emit(const Emitter& emitter);

	// Getter
	// Vec3 GetPos() const;
	// Vec3 GetScale() const;

	// Setter
	// void SetPos(const Vec3& newPos);
	// void SetScale(const Vec3& newScale);
	void SetCamera(Camera* newCamera);

private:
	ParticleCommon* particleCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;
	std::string textureFilePath_;

	uint32_t kNumMaxInstance = 256;
	uint32_t numInstance = 0; // 描画すべきインスタンス数

	uint32_t srvIndex_ = 0;

	// パーティクル
	std::list<Particle> particles_;

	Emitter emitter_ = {};
	AccelerationField accelerationField_ = {};

	std::vector<Vertex> vertices_;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_ = nullptr;
	std::unique_ptr<ConstantBuffer> materialResource_ = nullptr;
	std::unique_ptr<ConstantBuffer> instancingResource_ = nullptr;
	std::unique_ptr<IndexBuffer> indexBuffer_ = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Material* materialData_ = nullptr;
	ParticleForGPU* instancingData = nullptr;
};
