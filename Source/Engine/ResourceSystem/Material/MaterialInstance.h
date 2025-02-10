#pragma once
#include <string>
#include <ResourceSystem/Texture/Texture.h>

class MaterialInstance {
public:
	void SetTexture(const std::string& slot, Texture* texture) {
		textureSlots_[slot] = texture;
	}

	Texture* GetTexture(const std::string& slot) {
		auto it = textureSlots_.find(slot);
		return it != textureSlots_.end() ? it->second : nullptr;
	}

	void Apply(ID3D12GraphicsCommandList* commandList,
		UINT rootParameterIndex,
		ShaderResourceViewManager* srvManager,
		const std::vector<std::string>& textureOrder);
private:
	std::unordered_map<std::string, Texture*> textureSlots_;
};
