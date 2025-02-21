#include "VulkanRenderer.h"

#include "VulkanDevice.h"

VulkanRenderer::~VulkanRenderer() {
}

bool VulkanRenderer::Init(const RendererInitInfo& info) {
	if (info.enableDebugLayer) {

	}

	vulkanDevice_ = std::make_unique<VulkanDevice>();
	return vulkanDevice_ != nullptr;
}

void VulkanRenderer::Shutdown() {

}

BaseDevice* VulkanRenderer::GetDevice() {
	return vulkanDevice_.get();
}
