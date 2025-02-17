#include "VulkanRenderDevice.h"

#include "Lib/Utils/ClientProperties.h"
#include "Lib/Utils/StrUtils.h"

#include "SubSystem/Console/Console.h"

const std::vector<const char*> kValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

bool VulkanRenderDevice::Init() {
	// インスタンスの生成
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = kAppName.c_str();
	const auto versions = StrUtils::ParseVersion(kEngineVersion);
	appInfo.applicationVersion = VK_MAKE_VERSION(versions[0], versions[1], versions[2]);
	appInfo.pEngineName = kEngineName.c_str();
	appInfo.engineVersion = VK_MAKE_VERSION(versions[0], versions[1], versions[2]);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	auto extensions = GetRequiredExtensions();

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	instance_ = std::make_unique<VkInstance>();

	if (vkCreateInstance(&createInfo, nullptr, instance_.get()) != VK_SUCCESS) {
		Console::Print("Failed to create Vulkan instance.", kConTextColorError, Channel::RenderSystem);
		throw std::runtime_error("Failed to create Vulkan instance!");
	}

	// 拡張機能のサポートの確認
	uint32_t extensionCount = 0;
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	Console::Print("Available extensions:\n", kConTextColorWait, Channel::RenderSystem);
	for (const auto& extension : extensions) {
		Console::Print("\t" + std::string(extension.extensionName) + "\n", kConTextColorWait, Channel::RenderSystem);
	}

	// バリデーションレイヤーのサポートを確認
	if (kEnableValidationLayers && !CheckValidationLayerSupport()) {
		Console::Print("Validation layers requested, but not available!", kConTextColorError, Channel::RenderSystem);
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	return true;
}

bool VulkanRenderDevice::Shutdown() {

	vkDestroyInstance(*instance_, nullptr);

	Console::Print(
		"Shutdown VulkanRenderDevice.\n",
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: バリデーションレイヤーのサポートを確認
//-----------------------------------------------------------------------------
bool VulkanRenderDevice::CheckValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : kValidationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

std::vector<const char*> VulkanRenderDevice::GetRequiredExtensions() {
	std::vector<const char*> extensions;
	if (kEnableValidationLayers) {
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}