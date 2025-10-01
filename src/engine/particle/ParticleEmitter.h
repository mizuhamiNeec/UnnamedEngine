#pragma once
#include <string>

#include <engine/renderer/Structs.h>

class ParticleManager;

class ParticleEmitter {
public:
	void Init(ParticleManager* manager, const std::string& groupName);
	void Update(float deltaTime);

	void Emit();

private:
	ParticleManager* mParticleManager;
	std::string mGroupName;
	Emitter mEmitter;
};
