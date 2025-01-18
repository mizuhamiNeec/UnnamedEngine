#pragma once

#include <UnnamedResource/Manager/AudioManager.h>
#include <UnnamedResource/Manager/PipelineManager.h>
#include <UnnamedResource/Manager/TextureManager.h>

class ResourceManager {
public:
	static void Init();
	static void Shutdown();

	static TextureManager& GetTextureManager();
	static AudioManager& GetAudioManager();
	static PipelineManager& GetPipelineManager();
	static ShaderResourceViewManager& GetSrvManager();

private:
	static TextureManager textureManager_;
	static AudioManager audioManager_;
	static PipelineManager pipelineManager_;
	static ShaderResourceViewManager srvManager_;
};
