#pragma once

class BaseDevice {
public:
	virtual ~BaseDevice() = default;

	virtual bool CreateBuffer() = 0;
	virtual bool CreateTexture() = 0;
};
