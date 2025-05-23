#define SDL_MAIN_HANDLED
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "glm/glm.hpp"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#define VK_CHECK(x)                                                                       \
  do {                                                                                    \
    VkResult err = x;                                                                     \
    if(err != VK_SUCCESS) { std::cerr << "Detected Vulkan error: " << err << std::endl; } \
  } while(0)

#define SDL_CHECK(x)                                                                      \
  do {                                                                                    \
    if(!(bool) x) { std::cerr << "Detected SDL error: " << SDL_GetError() << std::endl; } \
  } while(0)

#ifdef _DEBUG
// VK_EXT_debug_utils
#  define vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_
inline PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_ = nullptr;
#  define vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_
inline PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ = nullptr;
#endif

bool quit = false;

const unsigned int FRAME_OVERLAP = 2;

VkInstance _instance;

#ifdef _DEBUG
VkDebugUtilsMessengerEXT _debugsMessenger;
#endif

VkPhysicalDevice _physicalDevice;
uint32_t _graphicsQueueFamilyIndex;
VkDevice _device;
VkQueue _graphicsQueue;
VmaAllocator _allocator;
SDL_Window* _window;
glm::ivec2 _windowExtent;
VkSurfaceKHR _surface;
VkSurfaceFormatKHR _surfaceFormat;

struct Frame {
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  VkSemaphore swapchainSemaphore;
  VkSemaphore renderSemaphore;
  VkFence renderFence;
};

uint32_t frameNumber;
std::array<std::shared_ptr<Frame>, FRAME_OVERLAP> frames;
VkSwapchainKHR _swapchain;
std::vector<VkImage> _swapchainImages;
std::vector<VkImageLayout> _swapchainImageLayouts;

VmaAllocation _drawImageAllocation;
VmaAllocationInfo _drawImageAllocationInfo;
VkFormat _colorFormat;
VkImage _drawImage;
VkExtent3D _drawImageExtent;
VkImageView _drawImageView;
VkImageLayout _drawImageLayout;
VkImageSubresourceRange _drawImageRange;
VkImageSubresourceLayers _drawImageLayers;

struct FragConstants {
  glm::vec3 iResolution; // viewport resolution (in pixels)
  float iTime; // shader playback time (in seconds)
  float iTimeDelta; // render time (in seconds)
  float iFrameRate; // shader frame rate
  int iFrame; // shader playback frame
  glm::vec4 iMouse; // mouse pixel coords. xy: current (if MLB down), zw: click
};

VkPipelineLayout _pipelineLayout;
VkPipeline _pipeline;

void createInstance() {
  VkApplicationInfo appInfo;
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "Singularity";
  appInfo.applicationVersion = 1;
  appInfo.pEngineName = "SingularityEngine";
  appInfo.engineVersion = 1;
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.pApplicationInfo = &appInfo;

  std::vector<const char*> extensions {};
  uint32_t sdlExtensionCount = 0;
  const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
  SDL_CHECK(sdlExtensions);
  for(uint32_t i = 0; i < sdlExtensionCount; i++) {
    extensions.push_back(sdlExtensions[i]);
  }

  std::vector<const char*> layers {};

#ifdef _DEBUG
  layers.push_back("VK_LAYER_KHRONOS_validation");
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  info.enabledExtensionCount = (uint32_t) extensions.size();
  info.ppEnabledExtensionNames = extensions.data();
  info.enabledLayerCount = (uint32_t) layers.size();
  info.ppEnabledLayerNames = layers.data();

  VK_CHECK(vkCreateInstance(&info, nullptr, &_instance));

  // fetch extension methods
#ifdef _DEBUG
  vkCreateDebugUtilsMessengerEXT_ = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    _instance, "vkCreateDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT_ = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    _instance, "vkDestroyDebugUtilsMessengerEXT");
#endif
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  std::cerr << "Severity: ";
  switch(messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      std::cerr << "verbose";
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      std::cerr << "info";
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      std::cerr << "warning";
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      std::cerr << "error";
      break;
    default:
      std::cerr << "NULL";
      break;
  }

  std::cerr << " | Type: ";
  switch(messageType) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
      std::cerr << "general";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
      std::cerr << "validation";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
      std::cerr << "performance";
      break;
    default:
      std::cerr << "NULL";
      break;
  }

  std::cerr << "\nValidation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

void createDebugsMessenger() {
  VkDebugUtilsMessengerCreateInfoEXT info;
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.pNext = nullptr;
  info.flags = 0;
  info.messageSeverity =
    /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/
    /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = debugCallback;
  info.pUserData = nullptr;

  VK_CHECK(vkCreateDebugUtilsMessengerEXT(_instance, &info, nullptr, &_debugsMessenger));
}
#endif

void createDevice() {
  // enumerate physical devices
  uint32_t deviceCount = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr));
  if(deviceCount == 0) { throw std::runtime_error("No Vulkan-compatible GPUs found."); }
  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  VK_CHECK(vkEnumeratePhysicalDevices(_instance, &deviceCount, physicalDevices.data()));

  // find discrete device with features and graphics queue
  _physicalDevice = VK_NULL_HANDLE;
  for(const auto& device : physicalDevices) {
    // check if is discrete
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    bool isDiscrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // check if has necessary features
    VkPhysicalDeviceVulkan13Features vk13Features {};
    vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features vk12Features {};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.pNext = &vk13Features;

    VkPhysicalDeviceFeatures2 features {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    features.pNext = &vk12Features;

    vkGetPhysicalDeviceFeatures2(device, &features);

    bool hasFeatures = vk13Features.dynamicRendering == VK_TRUE &&
                       vk13Features.synchronization2 == VK_TRUE &&
                       vk12Features.bufferDeviceAddress == VK_TRUE;

    // check if has a graphics queue
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    _graphicsQueueFamilyIndex = UINT32_MAX;
    bool hasGraphicsQueue = false;
    for(uint32_t i = 0; i < queueFamilyCount; ++i) {
      if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        _graphicsQueueFamilyIndex = i;
        hasGraphicsQueue = true;
        break;
      }
    }

    if(isDiscrete && hasFeatures && hasGraphicsQueue) {
      _physicalDevice = device;
      break;
    }
  }

  if(_physicalDevice == VK_NULL_HANDLE)
    throw std::runtime_error("Failed to select a suitable GPU.");

  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo;
  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.pNext = nullptr;
  queueInfo.flags = 0;
  queueInfo.queueFamilyIndex = _graphicsQueueFamilyIndex;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &queuePriority;

  VkPhysicalDeviceVulkan13Features vk13Features {};
  vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk13Features.dynamicRendering = VK_TRUE;
  vk13Features.synchronization2 = VK_TRUE;

  VkPhysicalDeviceVulkan12Features vk12Features {};
  vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12Features.pNext = &vk13Features;
  vk12Features.bufferDeviceAddress = VK_TRUE;

  VkPhysicalDeviceFeatures2 features {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
  features.pNext = &vk12Features;

  std::vector<const char*> extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pNext = &features;
  info.flags = 0;
  info.queueCreateInfoCount = 1;
  info.pQueueCreateInfos = &queueInfo;
  info.enabledLayerCount = 0;
  info.ppEnabledLayerNames = nullptr;
  info.enabledExtensionCount = (uint32_t) extensions.size();
  info.ppEnabledExtensionNames = extensions.data();
  info.pEnabledFeatures = nullptr;

  VK_CHECK(vkCreateDevice(_physicalDevice, &info, nullptr, &_device));
}

void createWindow() {
  SDL_DisplayID* displays = SDL_GetDisplays(nullptr);
  SDL_CHECK(displays);
  const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(displays[0]);

  _windowExtent = {dm->w, dm->h};

  _window = SDL_CreateWindow("Singularity", dm->w, dm->h,
    SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);

  if(!_window) { throw std::runtime_error(SDL_GetError()); }

  SDL_free(displays);
}

void createSwapchain() {
  // find surface format
  uint32_t surfaceFormatCount;
  VK_CHECK(
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, nullptr));
  std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(surfaceFormatCount);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount,
    availableSurfaceFormats.data()));

  const std::vector<VkFormat> preferredFormats = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UINT,
    VK_FORMAT_B8G8R8A8_SRGB};

  const std::vector<VkColorSpaceKHR> preferredColorSpaces = {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  _surfaceFormat.format = VK_FORMAT_UNDEFINED;
  for(const auto& colorSpace : preferredColorSpaces) {
    for(const auto& format : preferredFormats) {
      for(const auto& availableSurfaceFormat : availableSurfaceFormats) {
        if(availableSurfaceFormat.format == format &&
           availableSurfaceFormat.colorSpace == colorSpace) {
          _surfaceFormat = availableSurfaceFormat;
          break;
        }
      }
    }
  }
  if(_surfaceFormat.format == VK_FORMAT_UNDEFINED) {
    throw std::runtime_error("No prefered surface format available.");
  }

  // find present mode
  uint32_t presentModeCount = 0;
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount,
    nullptr));
  std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount,
    availablePresentModes.data()));

  const std::vector<VkPresentModeKHR> preferredPresentModes = {VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;

  for(const auto& preferredPresentMode : preferredPresentModes) {
    for(const auto& availablePresentMode : availablePresentModes) {
      if(preferredPresentMode == availablePresentMode) {
        presentMode = preferredPresentMode;
        break;
      }
    }
  }
  if(presentMode == VK_PRESENT_MODE_MAX_ENUM_KHR) {
    throw std::runtime_error("No preferred present mode available.");
  }

  // find image count
  VkSurfaceCapabilitiesKHR capabilities {};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &capabilities));

  uint32_t imageCount = capabilities.minImageCount + 1;
  if(capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  // create swapchain
  VkSwapchainCreateInfoKHR info;
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.pNext = nullptr;
  info.flags = 0;
  info.surface = _surface;
  info.minImageCount = imageCount;
  info.imageFormat = _surfaceFormat.format;
  info.imageColorSpace = _surfaceFormat.colorSpace;
  info.imageExtent = capabilities.currentExtent;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices = nullptr;
  info.preTransform = capabilities.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = presentMode;
  info.clipped = VK_TRUE;
  info.oldSwapchain = nullptr;

  VK_CHECK(vkCreateSwapchainKHR(_device, &info, nullptr, &_swapchain));

  uint32_t imgCount;
  VK_CHECK(vkGetSwapchainImagesKHR(_device, _swapchain, &imgCount, nullptr));
  _swapchainImages.resize(imgCount);
  VK_CHECK(vkGetSwapchainImagesKHR(_device, _swapchain, &imgCount, _swapchainImages.data()));

  for(int i = 0; i < imgCount; i++) {
    _swapchainImageLayouts.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
  }
}

void createFrames() {
  VkCommandPoolCreateInfo cmdPoolInfo;
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.pNext = nullptr;
  cmdPoolInfo.queueFamilyIndex = _graphicsQueueFamilyIndex;
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandBufferAllocateInfo cmdBufferAllocateInfo;
  cmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdBufferAllocateInfo.pNext = nullptr;
  cmdBufferAllocateInfo.commandBufferCount = 1;
  cmdBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VkSemaphoreCreateInfo semaphoreInfo;
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  semaphoreInfo.flags = 0;

  VkFenceCreateInfo fenceInfo;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = nullptr;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for(int i = 0; i < FRAME_OVERLAP; i++) {
    frames[i] = std::make_shared<Frame>();

    VK_CHECK(vkCreateCommandPool(_device, &cmdPoolInfo, nullptr, &frames[i]->commandPool));

    cmdBufferAllocateInfo.commandPool = frames[i]->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdBufferAllocateInfo, &frames[i]->commandBuffer));

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frames[i]->swapchainSemaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frames[i]->renderSemaphore));

    VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &frames[i]->renderFence));
  }

  frameNumber = 0;
}

void createAllocator() {
  VmaAllocatorCreateInfo info;
  info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  info.physicalDevice = _physicalDevice;
  info.device = _device;
  info.preferredLargeHeapBlockSize = 0;
  info.pAllocationCallbacks = nullptr;
  info.pDeviceMemoryCallbacks = nullptr;
  info.pHeapSizeLimit = nullptr;
  info.pVulkanFunctions = nullptr;
  info.instance = _instance;
  info.vulkanApiVersion = VK_API_VERSION_1_3;
  info.pTypeExternalMemoryHandleTypes = nullptr;

  VK_CHECK(vmaCreateAllocator(&info, &_allocator));
}

void createDrawImage() {
  // find color format
  const std::vector<VkFormat> preferredColorFormats = {
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_SNORM,
    VK_FORMAT_R8G8B8A8_USCALED,
    VK_FORMAT_R8G8B8A8_SSCALED,
  };

  _colorFormat = VK_FORMAT_UNDEFINED;
  for(const auto& format : preferredColorFormats) {
    VkImageFormatProperties properties;
    VkResult result = vkGetPhysicalDeviceImageFormatProperties(_physicalDevice, format,
      VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, &properties);
    if(result == VK_SUCCESS) {
      _colorFormat = format;
      break;
    }
  }
  if(_colorFormat == VK_FORMAT_UNDEFINED) {
    throw std::runtime_error("No prefered color image format available.");
  }

  // create image
  VkImageCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = _colorFormat;
  info.extent = _drawImageExtent;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices = nullptr;
  info.initialLayout = _drawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo;
  allocInfo.flags = 0;
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  allocInfo.preferredFlags = 0;
  allocInfo.memoryTypeBits = 0;
  allocInfo.pool = VK_NULL_HANDLE;
  allocInfo.pUserData = nullptr;
  allocInfo.priority = 0;

  VK_CHECK(vmaCreateImage(_allocator, &info, &allocInfo, &_drawImage, &_drawImageAllocation,
    &_drawImageAllocationInfo));

  _drawImageRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  _drawImageRange.baseMipLevel = 0;
  _drawImageRange.levelCount = 1;
  _drawImageRange.baseArrayLayer = 0;
  _drawImageRange.layerCount = 1;

  _drawImageLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  _drawImageLayers.mipLevel = 0;
  _drawImageLayers.baseArrayLayer = 0;
  _drawImageLayers.layerCount = 1;

  VkImageViewCreateInfo viewInfo;
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.pNext = nullptr;
  viewInfo.flags = 0;
  viewInfo.image = _drawImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = _colorFormat;
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.subresourceRange = _drawImageRange;

  VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &_drawImageView));
}

std::vector<uint32_t> readFileBytes(std::string path) {
  std::ifstream file(path, std::ios::ate | std::ios::in | std::ios::binary);

  if(!file.is_open()) { throw std::runtime_error("Failed to read file bytes."); }

  size_t fileSize = (size_t) file.tellg();
  std::vector<uint32_t> fileBytes(fileSize / sizeof(uint32_t));

  file.seekg(0);
  file.read((char*) fileBytes.data(), fileSize);
  file.close();

  return fileBytes;
}

void createPipeline(std::string fragShaderPath) {
  VkPipelineRenderingCreateInfo rendering;
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.pNext = nullptr;
  rendering.viewMask = 0;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &_colorFormat;
  rendering.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
  rendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

  VkPipelineVertexInputStateCreateInfo vertexInput;
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.pNext = nullptr;
  vertexInput.flags = 0;
  vertexInput.vertexBindingDescriptionCount = 0;
  vertexInput.pVertexBindingDescriptions = nullptr;
  vertexInput.vertexAttributeDescriptionCount = 0;
  vertexInput.pVertexAttributeDescriptions = nullptr;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly;
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.pNext = nullptr;
  inputAssembly.flags = 0;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineTessellationStateCreateInfo tessellation;
  tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessellation.pNext = nullptr;
  tessellation.flags = 0;
  tessellation.patchControlPoints = 0;

  VkViewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float) _drawImageExtent.width;
  viewport.height = (float) _drawImageExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = _drawImageExtent.width;
  scissor.extent.height = _drawImageExtent.height;

  VkPipelineViewportStateCreateInfo viewportState;
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;
  viewportState.flags = 0;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterization;
  rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.pNext = nullptr;
  rasterization.flags = 0;
  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.depthBiasConstantFactor = 0;
  rasterization.depthBiasClamp = 0;
  rasterization.depthBiasSlopeFactor = 0;
  rasterization.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample {};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = nullptr;
  multisample.flags = 0;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample.sampleShadingEnable = VK_FALSE;
  multisample.minSampleShading = 0;
  multisample.pSampleMask = nullptr;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable = VK_FALSE;

  VkPipelineDepthStencilStateCreateInfo depthStencil;
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.pNext = nullptr;
  depthStencil.flags = 0;
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {};
  depthStencil.back = {};
  depthStencil.minDepthBounds = 0.0f;
  depthStencil.maxDepthBounds = 1.0f;

  VkPipelineColorBlendAttachmentState attachment;
  attachment.blendEnable = VK_FALSE;
  attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachment.colorBlendOp = VK_BLEND_OP_ADD;
  attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlend;
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.pNext = nullptr;
  colorBlend.flags = 0;
  colorBlend.logicOpEnable = VK_FALSE;
  colorBlend.logicOp = VK_LOGIC_OP_COPY;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &attachment;
  colorBlend.blendConstants[0] = 0;
  colorBlend.blendConstants[1] = 0;
  colorBlend.blendConstants[2] = 0;
  colorBlend.blendConstants[3] = 0;

  VkPipelineDynamicStateCreateInfo dynamic;
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.pNext = nullptr;
  dynamic.flags = 0;
  dynamic.dynamicStateCount = 0;
  dynamic.pDynamicStates = nullptr;

  // create layout
  std::vector<VkPushConstantRange> ranges(1);
  ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  ranges[0].offset = 0;
  ranges[0].size = sizeof(FragConstants);

  VkPipelineLayoutCreateInfo layoutInfo;
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = nullptr;
  layoutInfo.flags = 0;
  layoutInfo.setLayoutCount = 0;
  layoutInfo.pSetLayouts = nullptr;
  layoutInfo.pushConstantRangeCount = (uint32_t) ranges.size();
  layoutInfo.pPushConstantRanges = ranges.data();

  VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_pipelineLayout));

  // load shaders
  std::vector<uint32_t> vertexShaderBytes = readFileBytes("../../shaders/vert.spv");

  VkShaderModuleCreateInfo vertexShaderModuleInfo;
  vertexShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  vertexShaderModuleInfo.pNext = nullptr;
  vertexShaderModuleInfo.flags = 0;
  vertexShaderModuleInfo.codeSize = vertexShaderBytes.size() * sizeof(uint32_t);
  ;
  vertexShaderModuleInfo.pCode = vertexShaderBytes.data();
  ;

  VkShaderModule vertexShaderModule;
  VK_CHECK(vkCreateShaderModule(_device, &vertexShaderModuleInfo, nullptr, &vertexShaderModule));

  std::vector<uint32_t> fragShaderBytes = readFileBytes(fragShaderPath);

  VkShaderModuleCreateInfo fragShaderModuleInfo;
  fragShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  fragShaderModuleInfo.pNext = nullptr;
  fragShaderModuleInfo.flags = 0;
  fragShaderModuleInfo.codeSize = fragShaderBytes.size() * sizeof(uint32_t);
  fragShaderModuleInfo.pCode = fragShaderBytes.data();

  VkShaderModule fragShaderModule;
  VK_CHECK(vkCreateShaderModule(_device, &fragShaderModuleInfo, nullptr, &fragShaderModule));

  std::vector<VkPipelineShaderStageCreateInfo> shaderInfos(2);
  shaderInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderInfos[0].pNext = nullptr;
  shaderInfos[0].flags = 0;
  shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderInfos[0].module = vertexShaderModule;
  shaderInfos[0].pName = "main";
  shaderInfos[0].pSpecializationInfo = nullptr;

  shaderInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderInfos[1].pNext = nullptr;
  shaderInfos[1].flags = 0;
  shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderInfos[1].module = fragShaderModule;
  shaderInfos[1].pName = "main";
  shaderInfos[1].pSpecializationInfo = nullptr;

  // create pipeline
  VkGraphicsPipelineCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = &rendering;
  info.flags = 0;

  info.stageCount = (uint32_t) shaderInfos.size();
  info.pStages = shaderInfos.data();

  info.pVertexInputState = &vertexInput;
  info.pInputAssemblyState = &inputAssembly;
  info.pTessellationState = &tessellation;
  info.pViewportState = &viewportState;
  info.pRasterizationState = &rasterization;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depthStencil;
  info.pColorBlendState = &colorBlend;
  info.pDynamicState = &dynamic;
  info.layout = _pipelineLayout;
  info.renderPass = VK_NULL_HANDLE;
  info.subpass = 0;
  info.basePipelineHandle = VK_NULL_HANDLE;
  info.basePipelineIndex = 0;

  VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &_pipeline));

  vkDestroyShaderModule(_device, vertexShaderModule, nullptr);
  vkDestroyShaderModule(_device, fragShaderModule, nullptr);
}

void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout& oldLayout,
  VkImageLayout newLayout) {
  VkImageMemoryBarrier2 imageBarrier;
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageBarrier.pNext = nullptr;

  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  imageBarrier.oldLayout = oldLayout;
  imageBarrier.newLayout = newLayout;
  oldLayout = newLayout;

  imageBarrier.srcQueueFamilyIndex = 0;
  imageBarrier.dstQueueFamilyIndex = 0;

  imageBarrier.image = image;
  imageBarrier.subresourceRange = _drawImageRange;

  VkDependencyInfo depInfo {};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;
  depInfo.dependencyFlags = 0;

  depInfo.memoryBarrierCount = 0;
  depInfo.pBufferMemoryBarriers = nullptr;

  depInfo.bufferMemoryBarrierCount = 0;
  depInfo.pBufferMemoryBarriers = nullptr;

  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imageBarrier;

  vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void blitDrawImage(VkCommandBuffer commandBuffer, VkImage dst) {
  VkImageBlit2 blitRegion;
  blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
  blitRegion.pNext = nullptr;

  blitRegion.srcSubresource = _drawImageLayers;

  blitRegion.srcOffsets[0].x = 0;
  blitRegion.srcOffsets[0].y = 0;
  blitRegion.srcOffsets[0].z = 0;
  blitRegion.srcOffsets[1].x = _drawImageExtent.width;
  blitRegion.srcOffsets[1].y = _drawImageExtent.height;
  blitRegion.srcOffsets[1].z = 1;

  blitRegion.dstSubresource = _drawImageLayers;

  blitRegion.dstOffsets[0].x = 0;
  blitRegion.dstOffsets[0].y = 0;
  blitRegion.dstOffsets[0].z = 0;
  blitRegion.dstOffsets[1].x = _windowExtent.x;
  blitRegion.dstOffsets[1].y = _windowExtent.y;
  blitRegion.dstOffsets[1].z = 1;

  VkBlitImageInfo2 blitInfo;
  blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
  blitInfo.pNext = nullptr;
  blitInfo.srcImage = _drawImage;
  blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blitInfo.dstImage = dst;
  blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blitInfo.regionCount = 1;
  blitInfo.pRegions = &blitRegion;
  blitInfo.filter = VK_FILTER_LINEAR;

  vkCmdBlitImage2(commandBuffer, &blitInfo);
}

int main(int argc, char* argv[]) {
  // read inputs
  std::string fragShaderPath = "";
  _drawImageExtent = {0, 0, 1};
  for(int i = 0; i < argc - 1; i++) {
    if(strcmp(argv[i], "--shader") == 0) {
      fragShaderPath = std::string(argv[i + 1]);
    } else if(strcmp(argv[i], "-xy")) {
      std::sscanf(argv[i + 1], "%dx%d", &_drawImageExtent.width, &_drawImageExtent.height);
    }
    //else if(strcmp(argv[i], "--help")) {
    //   std::cout << "Singularity" << std::endl << std::endl;
    //   std::cout << "Usage: Singularity [Options]" << std::endl << std::endl;
    //   std::cout << "\t--shader\tpath to fragment shader" << std::endl;
    //   return 0;
    // }
  }

  // verify inputs
  if(!std::filesystem::exists(fragShaderPath)) {
    std::cerr << "The path to this shader doesn't exist.\nIt must be relative to this program."
              << std::endl;
    return 0;
  }

  // begin program
  SDL_SetMainReady();
  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO));

  if(_drawImageExtent.width == 0 || _drawImageExtent.height == 0) {
    SDL_DisplayID* displays = SDL_GetDisplays(nullptr);
    SDL_CHECK(displays);
    const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(displays[0]);

    _drawImageExtent.width = (uint32_t) dm->w;
    _drawImageExtent.height = (uint32_t) dm->h;

    SDL_free(displays);
  }

  // init vulkan
  createWindow();
  createInstance();

#ifdef _DEBUG
  createDebugsMessenger();
#endif

  createDevice();
  vkGetDeviceQueue(_device, _graphicsQueueFamilyIndex, 0, &_graphicsQueue);

  SDL_CHECK(SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface));

  createSwapchain();
  createFrames();

  createAllocator();
  createDrawImage();

  createPipeline(fragShaderPath);

  uint32_t fps = 60; // frames per second
  uint32_t mspf = 1000 / (1000 * fps); // milli-seconds per frame
  uint32_t accruedMilliSeconds = 0;

  uint64_t now = 0;
  uint64_t last;
  uint32_t deltaTime;

  while(!quit) {
    // update time
    uint64_t last = now;
    now = SDL_GetTicks();
    deltaTime = now - last;

    // check fps
    accruedMilliSeconds += deltaTime;
    if(accruedMilliSeconds >= mspf) {
      accruedMilliSeconds -= mspf;

      // check events
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE ||
           event.type == SDL_EVENT_QUIT) {
          quit = true;
        }
      }

      // render frame
      std::shared_ptr<Frame> currentFrame = frames[frameNumber % FRAME_OVERLAP];

      // wait for gpu to finish rendering the last frame
      VK_CHECK(vkWaitForFences(_device, 1, &currentFrame->renderFence, VK_TRUE, 9999999999));
      VK_CHECK(vkResetFences(_device, 1, &currentFrame->renderFence));

      // reset command buffer
      VK_CHECK(vkResetCommandBuffer(currentFrame->commandBuffer, 0));
      // begin recording command buffer
      VkCommandBufferBeginInfo beginInfo;
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.pNext = nullptr;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      beginInfo.pInheritanceInfo = nullptr;
      VK_CHECK(vkBeginCommandBuffer(currentFrame->commandBuffer, &beginInfo));

      // clear image with color
      transitionImage(currentFrame->commandBuffer, _drawImage, _drawImageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      VkClearColorValue clearColor = {1, 1, 1, 1};
      vkCmdClearColorImage(currentFrame->commandBuffer, _drawImage, _drawImageLayout, &clearColor,
        1, &_drawImageRange);

      // transition image into writeable mode before rendering
      transitionImage(currentFrame->commandBuffer, _drawImage, _drawImageLayout,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      // begin a render pass connected to our draw image
      VkRenderingAttachmentInfo attachmentInfo;
      attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      attachmentInfo.pNext = nullptr;

      attachmentInfo.imageView = _drawImageView;
      attachmentInfo.imageLayout = _drawImageLayout;

      attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
      attachmentInfo.resolveImageView = VK_NULL_HANDLE;
      attachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentInfo.clearValue.color = {0, 0, 0, 0};

      VkRenderingInfo renderInfo;
      renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
      renderInfo.pNext = nullptr;
      renderInfo.flags = 0;
      renderInfo.renderArea.extent = {_drawImageExtent.width, _drawImageExtent.height};
      renderInfo.renderArea.offset = {0, 0};
      renderInfo.layerCount = 1;
      renderInfo.viewMask = 0;
      renderInfo.colorAttachmentCount = 1;
      renderInfo.pColorAttachments = &attachmentInfo;
      renderInfo.pDepthAttachment = nullptr;
      renderInfo.pStencilAttachment = nullptr;

      vkCmdBeginRendering(currentFrame->commandBuffer, &renderInfo);

      // draw shader
      vkCmdBindPipeline(currentFrame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

      FragConstants fragConstants;
      fragConstants.iResolution = {_drawImageExtent.width, _drawImageExtent.height,
        _drawImageExtent.depth};
      fragConstants.iTime = now / 1000.0f;
      fragConstants.iTimeDelta = deltaTime / 1000.0f;
      fragConstants.iFrameRate = fps;
      fragConstants.iFrame = frameNumber;
      fragConstants.iMouse = {0, 0, 0, 0};

      vkCmdPushConstants(currentFrame->commandBuffer, _pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FragConstants),
        &fragConstants);

      vkCmdDraw(currentFrame->commandBuffer, 6, 1, 0, 0);

      // end rendering
      vkCmdEndRendering(currentFrame->commandBuffer);

      // request an image from the swapchain
      uint32_t swapchainImageIndex;
      VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 9999999999,
        currentFrame->swapchainSemaphore, nullptr, &swapchainImageIndex));

      // transition the draw image and the swapchain image into their correct transfer layouts
      transitionImage(currentFrame->commandBuffer, _drawImage, _drawImageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      transitionImage(currentFrame->commandBuffer, _swapchainImages[swapchainImageIndex],
        _swapchainImageLayouts[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      // copy drawimage to swapchain image
      blitDrawImage(currentFrame->commandBuffer, _swapchainImages[swapchainImageIndex]);

      // make the swapchain image into presentable mode
      transitionImage(currentFrame->commandBuffer, _swapchainImages[swapchainImageIndex],
        _swapchainImageLayouts[swapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

      // end the command buffer
      VK_CHECK(vkEndCommandBuffer(currentFrame->commandBuffer));

      // submit command buffer to queue and execute it
      VkSemaphoreSubmitInfo waitInfo;
      waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      waitInfo.pNext = nullptr;
      waitInfo.semaphore = currentFrame->swapchainSemaphore;
      waitInfo.value = 1;
      waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
      waitInfo.deviceIndex = 0;

      VkSemaphoreSubmitInfo signalInfo;
      signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      signalInfo.pNext = nullptr;
      signalInfo.semaphore = currentFrame->renderSemaphore;
      signalInfo.value = 1;
      signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
      signalInfo.deviceIndex = 0;

      VkCommandBufferSubmitInfo cmdBufferInfo;
      cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      cmdBufferInfo.pNext = nullptr;
      cmdBufferInfo.commandBuffer = currentFrame->commandBuffer;
      cmdBufferInfo.deviceMask = 0;

      VkSubmitInfo2 submitInfo;
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submitInfo.pNext = nullptr;
      submitInfo.flags = 0;
      submitInfo.waitSemaphoreInfoCount = 1;
      submitInfo.pWaitSemaphoreInfos = &waitInfo;
      submitInfo.signalSemaphoreInfoCount = 1;
      submitInfo.pSignalSemaphoreInfos = &signalInfo;
      submitInfo.commandBufferInfoCount = 1;
      submitInfo.pCommandBufferInfos = &cmdBufferInfo;
      VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, currentFrame->renderFence));

      // present image to screen
      VkPresentInfoKHR presentInfo;
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.pNext = nullptr;
      presentInfo.pWaitSemaphores = &currentFrame->renderSemaphore;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pSwapchains = &_swapchain;
      presentInfo.swapchainCount = 1;
      presentInfo.pImageIndices = &swapchainImageIndex;
      presentInfo.pResults = nullptr;
      VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

      // increase the number of frames drawn
      frameNumber++;
    }
  }

  VK_CHECK(vkDeviceWaitIdle(_device));

  vkDestroyPipeline(_device, _pipeline, nullptr);
  vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
  vkDestroyImageView(_device, _drawImageView, nullptr);
  vmaDestroyImage(_allocator, _drawImage, _drawImageAllocation);
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
  for(auto& frame : frames) {
    vkDestroyCommandPool(_device, frame->commandPool, nullptr);
    vkDestroySemaphore(_device, frame->renderSemaphore, nullptr);
    vkDestroySemaphore(_device, frame->swapchainSemaphore, nullptr);
    vkDestroyFence(_device, frame->renderFence, nullptr);
  }
  vkDestroySurfaceKHR(_instance, _surface, nullptr);
  SDL_DestroyWindow(_window);
  vmaDestroyAllocator(_allocator);
  vkDestroyDevice(_device, nullptr);

#ifdef _DEBUG
  vkDestroyDebugUtilsMessengerEXT(_instance, _debugsMessenger, nullptr);
#endif

  vkDestroyInstance(_instance, nullptr);

  SDL_Quit();
}