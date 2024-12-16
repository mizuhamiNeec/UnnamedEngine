#pragma once
#include <string>

#include "../Lib/Structs/Structs.h"

class ParticleManager;

class ParticleEmitter {
public:
	void Init(ParticleManager* manager, const std::string& groupName);
	void Update(float deltaTime);

	void Emit();

private:
	ParticleManager* particleManager_;
	std::string groupName_;
	Emitter emitter_;
};
