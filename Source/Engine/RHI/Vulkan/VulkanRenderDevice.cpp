#include "VulkanRenderDevice.h"

#include <format>

#include <Lib/Utils/ClientProperties.h>
#include <Lib/Utils/StrUtils.h>

#include <SubSystem/Console/Console.h>

#include "Window/Window.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_win32.h>
#endif

#include <vulkan/vulkan_core.h>

const std::vector<const char*> kValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

static VkResult CreateDebugUtilsMessengerEXT(
	const VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger
) {
	const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
	const VkInstance instance,
	const VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator) {
	auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	[[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	[[maybe_unused]] void* pUserData) {
	const std::string message = "Validation layer: " + std::string(pCallbackData->pMessage) + "\n";
	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		Console::Print(message, kConTextColorCompleted, Channel::RenderSystem);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		Console::Print(message, kConTextColorCompleted, Channel::RenderSystem);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		Console::Print(message, kConTextColorWarning, Channel::RenderSystem);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		Console::Print(message, kConTextColorError, Channel::RenderSystem);
		break;
	}

	return VK_FALSE;
}

bool VulkanRenderDevice::Init() {
	//-------------------------------------------------------------------------
	// Vulkanインスタンスの作成
	//-------------------------------------------------------------------------
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = kAppName.c_str();
	const auto ver = StrUtils::ParseVersion(kEngineVersion);
	if (ver.size() == 3) {
		appInfo.applicationVersion = VK_MAKE_VERSION(ver[0], ver[1], ver[2]);
	} else {
		// バージョンが無効
		Console::Print(
			"Invalid version format.\n",
			kConTextColorError,
			Channel::RenderSystem
		);
	}
	appInfo.pEngineName = kEngineName.c_str();
	appInfo.engineVersion = VK_MAKE_VERSION(ver[0], ver[1], ver[2]);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	// 拡張機能と検証レイヤーを取得
	auto extensions = GetRequiredExtensions();

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if (kEnableValidationLayers) {
		if (!CheckValidationLayerSupport()) {
			Console::Print(
				"Validation layers requested, but not available!\n",
				kConTextColorError,
				Channel::RenderSystem
			);
			throw std::runtime_error("Validation layers requested, but not available!");
		}
		createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		createInfo.ppEnabledLayerNames = kValidationLayers.data();

		// コールバックの設定
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = DebugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// インスタンスの作成
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
	if (result != VK_SUCCESS) {
		Console::Print(
			"Failed to create Vulkan instance.\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	// デバッグメッセンジャーの設定
	if (kEnableValidationLayers) {
		if (!SetupDebugMessenger()) {
			return false;
		}
	}

	// 物理デバイスの選択
	if (!PickPhysicalDevice()) {
		Console::Print(
			"Failed to find a suitable GPU!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	// ロジカルデバイスの作成とグラフィックスキューの取得
	if (!CreateLogicalDevice()) {
		Console::Print(
			"Failed to create logical device!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	return true;
}

bool VulkanRenderDevice::Shutdown() {

	if (device_ != VK_NULL_HANDLE) {
		vkDestroyDevice(device_, nullptr);
		device_ = VK_NULL_HANDLE;
	}

	if (debugMessenger_ != VK_NULL_HANDLE) {
		DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
		debugMessenger_ = VK_NULL_HANDLE;
	}

	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
		surface_ = VK_NULL_HANDLE;
	}

	if (instance_ != VK_NULL_HANDLE) {
		vkDestroyInstance(instance_, nullptr);
		instance_ = VK_NULL_HANDLE;
	}

	Console::Print(
		"Shutdown VulkanRenderDevice.\n",
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	return true;
}

bool VulkanRenderDevice::InitializeSurface() {
	if (instance_ == VK_NULL_HANDLE) {
		Console::Print(
			"Vulkan instance is not initialized!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	if (Window::GetWindowHandle() == nullptr) {
		Console::Print(
			"Invalid window handle!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = Window::GetWindowHandle();
	createInfo.hinstance = GetModuleHandle(nullptr);

	VkResult result = vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
	if (result != VK_SUCCESS) {
		std::string message = "Vulkan instance creation returned: " + std::to_string(result) + "\n";
		Console::Print(
			message,
			kConTextColorCompleted,
			Channel::RenderSystem
		);
		return false;
	}

	Console::Print("Vulkan surface created.\n",
		kConTextColorCompleted,
		Channel::RenderSystem
	);
	return true;
}

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

#ifdef _WIN32
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

	if (kEnableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

bool VulkanRenderDevice::SetupDebugMessenger() {
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr;

	VkResult result = CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_);
	if (result != VK_SUCCESS) {
		Console::Print(
			"Failed to set up debug messenger!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}
	return true;
}

bool VulkanRenderDevice::PickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
	if (deviceCount == 0) {
		Console::Print(
			"Failed to find GPUs with Vulkan support!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

	for (const auto& device : devices) {
		// グラフィックスキューを持つデバイスを選択
		if (FindQueueFamilies(device, graphicsQueueFamilyIndex_)) {
			physicalDevice_ = device;
			break;
		}
	}

	if (physicalDevice_ == VK_NULL_HANDLE) {
		Console::Print(
			"Failed to find a suitable GPU!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}
	return true;
}

bool VulkanRenderDevice::FindQueueFamilies(const VkPhysicalDevice device, uint32_t& outQueueFamilyIndex) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	if (queueFamilyCount == 0) {
		return false;
	}

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
	for (uint32_t i = 0; i < queueFamilyCount; ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			outQueueFamilyIndex = i;
			return true;
		}
	}
	return false;
}

bool VulkanRenderDevice::CreateLogicalDevice() {
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures = {};
	// アプリケーションで必要な機能を有効にする
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pEnabledFeatures = &deviceFeatures;

	// スワップチェーン拡張など、必要なデバイス拡張機能を設定
	std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (kEnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
		createInfo.ppEnabledLayerNames = kValidationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
	if (result != VK_SUCCESS) {
		Console::Print(
			"Failed to create logical device!\n",
			kConTextColorError,
			Channel::RenderSystem
		);
		return false;
	}

	vkGetDeviceQueue(device_, graphicsQueueFamilyIndex_, 0, &graphicsQueue_);
	return true;
}
