#pragma once
#include <d3d12.h>
#include <string>
#include <vector>

#include "SubMesh.h"

class StaticMesh {
public:
	StaticMesh(std::string name) : name_(std::move(name)) {}
	~StaticMesh() = default;

	void AddSubMesh(std::unique_ptr<SubMesh> subMesh);
	const std::vector<std::unique_ptr<SubMesh>>& GetSubMeshes() const;
	std::string GetName() const;
	void Render(ID3D12GraphicsCommandList* commandList) const;

	void ReleaseResource();

private:
	std::string name_;
	std::vector<std::unique_ptr<SubMesh>> subMeshes_;
};

