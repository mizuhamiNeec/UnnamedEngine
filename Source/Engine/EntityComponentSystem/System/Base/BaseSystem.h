#pragma once

class BaseSystem {
public:
	virtual ~BaseSystem() = default;
	virtual void Initialize() = 0;
	virtual void Update(float deltaTime) = 0;
	virtual void Terminate() = 0;
};

