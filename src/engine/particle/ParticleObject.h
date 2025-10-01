#pragma once
#include <list>
#include <memory>

#include <engine/renderer/ConstantBuffer.h>
#include <engine/renderer/IndexBuffer.h>
#include <engine/renderer/Structs.h>

class SrvManager;
class CameraComponent;
struct Vertex;
struct ParticleForGPU;
class Camera;
class ParticleManager;

enum class BillboardType {
	None, // ビルボードなし
	XY,   // XY平面にビルボード
	XZ,   // XZ平面にビルボード
	YZ,   // YZ平面にビルボード
	All   // 全方向にビルボード
};

class ParticleObject {
public:
	~ParticleObject() = default;

	void Init(ParticleManager*   particleCommon,
	          const std::string& textureFilePath);
	void Update(float deltaTime);
	void Draw() const;

	void Shutdown();

	static Particle MakeNewParticle(const Vec3& pos, const Vec3& vel,
	                                const Vec3& drag, const Vec3& gravity,
	                                Vec4        startColor, Vec4 endColor, const
	                                Vec3&       startSize, const Vec3& endSize);

	std::list<Particle> Emit(
		const Emitter&        emitter, int      shapeType, float     coneAngle,
		const Vec3&           drag, const Vec3& gravity, const Vec3& velocity,
		Vec4                  startColor, Vec4
		endColor, const Vec3& startSize, const Vec3& endSize
	);

	void SetCamera(CameraComponent* newCamera);
	Vec3 GeneratePosition(const Vec3& emitterPosition, int shapeType) const;
	static Vec3 GenerateConeVelocity(float coneAngle);

	void EmitParticlesAtPosition(const Vec3& position, int shapeType,
	                             float coneAngle, const Vec3& drag,
	                             const Vec3& gravity, const Vec3& velocity,
	                             uint32_t count, Vec4 startColor, Vec4 endColor,
	                             const Vec3& startSize, const
	                             Vec3& endSize);

	void SetBillboardType(const BillboardType type) {
		mBillboardType = type;
	}

	[[nodiscard]] BillboardType GetBillboardType() const {
		return mBillboardType;
	}

private:
	struct Material {
		Vec4    color;
		int32_t enableLighting;
		Vec3    padding;
		Mat4    uvTransform;
		float   shininess;
		Vec3    specularColor;
	};

	BillboardType mBillboardType = BillboardType::All;

	ParticleManager* mParticleCommon = nullptr;
	SrvManager*      mSrvManager     = nullptr;
	CameraComponent* mCamera         = nullptr;
	std::string      mTextureFilePath;

	uint32_t kNumMaxInstance = 16385; // 最大インスタンス数
	uint32_t mNumInstance    = 0;     // 描画すべきインスタンス数

	uint32_t mSrvIndex = 0;

	// パーティクル
	std::list<Particle> mParticles;

	Emitter           mEmitter           = {};
	AccelerationField mAccelerationField = {};

	std::unique_ptr<ConstantBuffer> mAterialResource    = nullptr;
	std::unique_ptr<ConstantBuffer> mInstancingResource = nullptr;
	std::unique_ptr<IndexBuffer>    mIndexBuffer        = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex*         mVertexData     = nullptr;
	uint32_t*       mIndexData      = nullptr;
	Material*       mAterialData    = nullptr;
	ParticleForGPU* mInstancingData = nullptr;

	bool mEnableGravity           = true;
	bool mEnableAccelerationField = true;
	bool mEnableDrag              = true;
};
