
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"
#include "resource.h"
#include "volk.h"
#include <stddef.h>
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "asset_loader.h"
#include "glm/common.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_int2.hpp"
#include <cstdint>
#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include <vulkan/vulkan_core.h>
// #include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #include <vulkan/vulkan_core.h>
// #include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <string.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "vulkan_renderer.h"
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vkutils.h"

#include "asset_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#ifdef USE_OPENXR
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#endif
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>



#endif

#define WINDOW_BASE_WIDTH 1280
#define WINDOW_BASE_HEIGHT 720



const char *projectName = "Rory's Renderer!!!";
const bool enableValidationLayers = false;
const char *validationLayers = "VK_LAYER_KHRONOS_validation";

uint32_t windowWidth = WINDOW_BASE_WIDTH;
uint32_t windowHeight = WINDOW_BASE_HEIGHT;

VkInstance instance;
VkDevice device;
VkPhysicalDevice physicalDevice;
VkQueue graphicsQueue;
uint32_t queueFamily = 0;
VmaAllocator allocator;

GLFWwindow *window;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
VkSurfaceCapabilitiesKHR surfaceCaps = {};
VkDebugUtilsMessengerEXT debugMessenger;

VkImage *swapchainImages = NULL;
VkImageView *swapchainImageViews = NULL;
uint32_t swapchainImageCount = 0;
VkImage depthImage;
VkImageView depthImageView;
VmaAllocation depthImageAllocation;

VkCommandPool commandPool;
VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];

glm::vec4 frustumPlanes[6];

// Sync Objects
VkSemaphore imageAcquiredSemaphores[FRAMES_IN_FLIGHT];
VkSemaphore *renderCompleteSemaphores;
VkFence fences[FRAMES_IN_FLIGHT];

Pipeline *pipeline = NULL;
Pipeline *pipelineLayout = NULL;

uint32_t frameIndex = 0;
uint32_t imageIndex = 0;

struct PushConstants
{
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    uint32_t frameIndex;
    uint32_t unused[3];
#else

#endif
};

#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
struct DescriptorHeapData
{
    /*
        [reserved]
        [PerFrame 0]
        [PerFrame 1]
        [Textures]
    */
    uint32_t alignment;
    uint32_t perFrameOffset;
    uint32_t textureOffset;
    VkDeviceSize resourceHeapSize;
    VkBuffer resourceHeapBuffer;
    VmaAllocation resourceHeapAllocation;
    VmaAllocationInfo resourceHeapAllocInfo;
    VkDeviceAddress resourceHeapDeviceAddress;
    VkDescriptorSetAndBindingMappingEXT bindingMappings[2];
    /*
        [reserved]
        [Samplers]
    */
    uint32_t samplerLinearOffset;
    uint32_t samplerNearestOffset;
    VkDeviceSize samplerHeapSize;
    VkBuffer samplerHeapBuffer;
    VmaAllocation samplerHeapAllocation;
    VmaAllocationInfo samplerHeapAllocInfo;
    VkDeviceAddress samplerHeapDeviceAddress;
};

// Descriptors
DescriptorHeapData heapData;
#else

VkDescriptorPool descriptorPool;
VkDescriptorPool imguiDescriptorPool;

VkDescriptorSetLayout descriptorSetLayoutTex;
VkDescriptorSet descriptorSetTex;

uint32_t renderableIndices[MAX_RENDERABLES];

BufferID renderableDataBuffer;
BufferID indirectDataBuffer;
BufferID renderableIndexBuffer;

uint32_t renderableCount = 0;
struct MeshOffsetsPushConstants
{
    VkDeviceAddress indirectAddress;
    uint32_t numMeshes;
};

struct IncrementIndirectPushConstants
{
    VkDeviceAddress globalDataAddress;
    VkDeviceAddress indirectAddress;
    VkDeviceAddress renderableDataAddress;
    uint32_t numRenderables;
};

struct CalculateIndicesPushConstants
{
    VkDeviceAddress indirectAddress;
    VkDeviceAddress renderableDataAddress;
    VkDeviceAddress renderableIndexAddress;
    uint32_t numRenderables;
};

struct MatGenericPushConstants
{
    VkDeviceAddress globalDataAddress;
    VkDeviceAddress renderableDataAddress;
    VkDeviceAddress renderableIndexAddress;
};

PipelineID pipelineClearIndirectCS;
PipelineID pipelineIncrementIndirectCS;
PipelineID pipelineMeshOffsetsIndirectCS;
PipelineID pipelineIndicesIndirectCS;

#endif

BufferID globalDataBuffer;

#ifdef USE_OPENXR
    XrInstance xrInstance = XR_NULL_HANDLE;
    XrDebugUtilsMessengerEXT xrDebugUtilsMessenger = XR_NULL_HANDLE;
    XrSystemId xrSystemID = XR_NULL_SYSTEM_ID;
    XrSystemProperties xrSystemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
    XrEventDataBuffer xrEventData{XR_TYPE_EVENT_DATA_BUFFER};
    XrSession xrSession = XR_NULL_HANDLE;
    XrSessionState xrSessionState = XR_SESSION_STATE_UNKNOWN;
    XrViewConfigurationType xrApplicationViewConfigurations[] = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    XrViewConfigurationType * xrViewConfigurations;
    XrViewConfigurationType xrViewConfiguration;
    XrViewConfigurationView * xrViewConfigurationViews;
    uint32_t xrViewConfigurationViewCount = 0;
    struct XrSwapchainInfo
    {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
        VkImageView * imageViews;
        VkImage * images;
    };
    XrSwapchainInfo * xrColorSwapchainInfos;
    XrSwapchainInfo * xrDepthSwapchainInfos;

    XrSpace xrLocalSpace = XR_NULL_HANDLE;
    struct RenderLayerInfo {
            XrTime predictedDisplayTime;
            XrCompositionLayerBaseHeader ** layers;
            XrCompositionLayerProjection layerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            XrCompositionLayerProjectionView * layerProjectionViews;
    };

    bool xrSessionRunning = false;
    bool xrApplicationRunning = false;
    std::vector<std::string> xrVulkanInstanceExtensions;
    std::vector<std::string> xrVulkanDeviceExtensions;

    static void ParseSpaceDelimitedExtensions(const char *extStr, std::vector<std::string> &out)
    {
        out.clear();
        if (!extStr) return;
        std::istringstream iss(extStr);
        std::string token;
        while (iss >> token)
        {
            out.push_back(token);
        }
    }
    #define OPENXR_CHECK(x, y)                                                                         \
    {                                                                                              \
        XrResult result = (x);                                                                     \
        if (!XR_SUCCEEDED(result)) {                                                               \
            char resultString[XR_MAX_RESULT_STRING_SIZE];                                          \
            xrResultToString(xrInstance, result, resultString);                                  \
            spdlog::error("OPENXR_CHECK failed: {} ({})", y, resultString);                          \
        }                                                                                          \
    }
#endif

// Properties and Limits
VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProperties;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{

    spdlog::error("validation layer: {}", pCallbackData->pMessage);

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

void GfxSetupDebugMessanger()
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = NULL;
    CreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &debugMessenger);
}

void GfxSetPipeline(Pipeline *_pipeline)
{
    pipeline = _pipeline;
}
void GfxCreateInstance()
{
    volkInitialize();
    VkApplicationInfo appInfo =
        {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = projectName,
            .apiVersion = VK_API_VERSION_1_3};

    uint32_t instanceExtensionCountGlfw = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCountGlfw);
    std::vector<std::string> enabledExtStorage;
    auto addUniqueExt = [&](const char *name) {
        if (name && std::find(enabledExtStorage.begin(), enabledExtStorage.end(), name) == enabledExtStorage.end())
            enabledExtStorage.emplace_back(name);
    };
    for (uint32_t i = 0; i < instanceExtensionCountGlfw; ++i)
    {
        addUniqueExt(glfwExtensions[i]);
    }
    addUniqueExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#ifdef USE_OPENXR
    for (const auto &ext : xrVulkanInstanceExtensions)
    {
        addUniqueExt(ext.c_str());
    }
#endif
    std::vector<const char *> instanceExtensionPtrs;
    instanceExtensionPtrs.reserve(enabledExtStorage.size());
    for (const auto &ext : enabledExtStorage)
    {
        instanceExtensionPtrs.push_back(ext.c_str());
    }

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensionPtrs.size());
    createInfo.ppEnabledExtensionNames = instanceExtensionPtrs.data();

    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayers;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    spdlog::info("Instance Extensions (Count: {}):", createInfo.enabledExtensionCount);
    for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
    {
        spdlog::info("  - {}", createInfo.ppEnabledExtensionNames[i]);
    }
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));
    spdlog::info("Vulkan Instance Created");


    volkLoadInstance(instance);
}
#ifdef USE_OPENXR
XrBool32 GfxOpenXRMessageCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageType, const XrDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    spdlog::info("{}", pCallbackData->message);
    return true;
}
void GfxCreateOpenXRInstance()
{
    uint32_t availableExtCount = 0;
    XrResult extEnumRes = xrEnumerateInstanceExtensionProperties(nullptr, 0, &availableExtCount, nullptr);
    if (XR_FAILED(extEnumRes))
    {
        spdlog::error("xrEnumerateInstanceExtensionProperties failed (XrResult: {})", static_cast<int>(extEnumRes));
        return;
    }

    bool hasDebugUtils = false;
    bool hasVulkanEnable = false;
    if (availableExtCount > 0)
    {
        XrExtensionProperties *availableExts = (XrExtensionProperties *)malloc(sizeof(XrExtensionProperties) * availableExtCount);
        for (uint32_t i = 0; i < availableExtCount; ++i)
        {
            availableExts[i] = {XR_TYPE_EXTENSION_PROPERTIES};
        }
        if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, availableExtCount, &availableExtCount, availableExts)))
        {
            for (uint32_t i = 0; i < availableExtCount; ++i)
            {
                if (strcmp(availableExts[i].extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                    hasDebugUtils = true;
                if (strcmp(availableExts[i].extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) == 0)
                    hasVulkanEnable = true;
            }
        }
        free(availableExts);
    }

    if (!hasVulkanEnable)
    {
        spdlog::error("OpenXR runtime does not support required extension {}", XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
        return;
    }

    const char *instanceExtensions[2];
    uint32_t instanceExtensionCount = 0;
    instanceExtensions[instanceExtensionCount++] = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
    if (hasDebugUtils)
    {
        instanceExtensions[instanceExtensionCount++] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    XrApplicationInfo appInfo = {};
    strncpy(appInfo.applicationName, "XR Demo", XR_MAX_APPLICATION_NAME_SIZE - 1);
    appInfo.applicationVersion = 1;
    strncpy(appInfo.engineName, "Pretty Neat Renderer", XR_MAX_ENGINE_NAME_SIZE - 1);
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_API_VERSION_1_0;

    XrInstanceCreateInfo instanceCreateInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .createFlags = 0,
        .applicationInfo = appInfo,
        .enabledExtensionCount = instanceExtensionCount,
        .enabledExtensionNames = instanceExtensions
    };
    XrResult createRes = xrCreateInstance(&instanceCreateInfo, &xrInstance);
    if (XR_FAILED(createRes) || xrInstance == XR_NULL_HANDLE)
    {
        spdlog::error("xrCreateInstance failed (XrResult: {})", static_cast<int>(createRes));
        xrInstance = XR_NULL_HANDLE;
        return;
    }

    // Create Debug Messenger
    if (hasDebugUtils)
    {
        XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugUtilsMessengerCI.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
        debugUtilsMessengerCI.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)GfxOpenXRMessageCallback;
        debugUtilsMessengerCI.userData = nullptr;

        PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
        XrResult procRes = xrGetInstanceProcAddr(xrInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&xrCreateDebugUtilsMessengerEXT);
        if (XR_SUCCEEDED(procRes) && xrCreateDebugUtilsMessengerEXT != nullptr)
        {
            OPENXR_CHECK(xrCreateDebugUtilsMessengerEXT(xrInstance, &debugUtilsMessengerCI, &xrDebugUtilsMessenger), "xrCreateDebugUtilsMessengerEXT");
        }
    }

    // Obtain System ID
    XrSystemGetInfo systemGetInfo = {
        .type = XR_TYPE_SYSTEM_GET_INFO,
        .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
    };
    XrResult sysRes = xrGetSystem(xrInstance, &systemGetInfo, &xrSystemID);
    if (XR_FAILED(sysRes))
    {
        OPENXR_CHECK(sysRes, "xrGetSystem");
        xrSystemID = XR_NULL_SYSTEM_ID;
        return;
    }
    OPENXR_CHECK(xrGetSystemProperties(xrInstance, xrSystemID, &xrSystemProperties), "xrGetSystemProperties");

    PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;
    XrResult reqProcRes = xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsRequirementsKHR);
    if (XR_FAILED(reqProcRes) || xrGetVulkanGraphicsRequirementsKHR == nullptr)
    {
        OPENXR_CHECK(reqProcRes, "xrGetInstanceProcAddr(xrGetVulkanGraphicsRequirementsKHR)");
        return;
    }
    XrGraphicsRequirementsVulkanKHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    XrResult reqRes = xrGetVulkanGraphicsRequirementsKHR(xrInstance, xrSystemID, &graphicsRequirements);
    if (XR_FAILED(reqRes))
    {
        OPENXR_CHECK(reqRes, "xrGetVulkanGraphicsRequirementsKHR");
        return;
    }

    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanInstanceExtensionsKHR)) && xrGetVulkanInstanceExtensionsKHR)
    {
        uint32_t bufCount = 0;
        if (XR_SUCCEEDED(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, 0, &bufCount, nullptr)) && bufCount > 0)
        {
            std::string buf(bufCount, '\0');
            if (XR_SUCCEEDED(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, bufCount, &bufCount, buf.data())))
            {
                ParseSpaceDelimitedExtensions(buf.c_str(), xrVulkanInstanceExtensions);
            }
        }
    }

    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanDeviceExtensionsKHR)) && xrGetVulkanDeviceExtensionsKHR)
    {
        uint32_t bufCount = 0;
        if (XR_SUCCEEDED(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, 0, &bufCount, nullptr)) && bufCount > 0)
        {
            std::string buf(bufCount, '\0');
            if (XR_SUCCEEDED(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, bufCount, &bufCount, buf.data())))
            {
                ParseSpaceDelimitedExtensions(buf.c_str(), xrVulkanDeviceExtensions);
            }
        }
    }
}

void GfxCreateOpenXRSession()
{
    if (xrInstance == XR_NULL_HANDLE || xrSystemID == XR_NULL_SYSTEM_ID)
        return;

    XrGraphicsBindingVulkanKHR graphicsBinding = {
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
        .instance = instance,
        .physicalDevice = physicalDevice,
        .device = device,
        .queueFamilyIndex = queueFamily,
        .queueIndex = 0
    };
    XrSessionCreateInfo sessionCreateInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = xrSystemID
    };
    XrResult sessionRes = xrCreateSession(xrInstance, &sessionCreateInfo, &xrSession);
    if (XR_FAILED(sessionRes))
    {
        OPENXR_CHECK(sessionRes, "xrCreateSession");
        xrSession = XR_NULL_HANDLE;
        return;
    }
    xrSessionRunning = false;
    xrApplicationRunning = true;

    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCreateInfo.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    OPENXR_CHECK(xrCreateReferenceSpace(xrSession, &referenceSpaceCreateInfo, &xrLocalSpace), "xrCreateReferenceSpace");

    GfxGetViewConfigurationViews();
}
void GfxGetViewConfigurationViews()
{
    uint32_t xrViewConfigurationCount = 0;
    xrEnumerateViewConfigurations(xrInstance, xrSystemID, 0, &xrViewConfigurationCount, nullptr);
    xrViewConfigurations = (XrViewConfigurationType*) malloc(sizeof(XrViewConfigurationType) * xrViewConfigurationCount);
    xrEnumerateViewConfigurations(xrInstance, xrSystemID, xrViewConfigurationCount, &xrViewConfigurationCount, xrViewConfigurations);

    xrViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    for (uint32_t i = 0; i < 2 && xrViewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM; ++i)
    {
        for (uint32_t j = 0; j < xrViewConfigurationCount; ++j)
        {
            if (xrViewConfigurations[j] == xrApplicationViewConfigurations[i]) {
                xrViewConfiguration = xrViewConfigurations[j];
                break;
            }
        }
    }
    if (xrViewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM)
    {
        spdlog::error("Failed to find a view configuration type, defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO");
        xrViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    }
    
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, xrViewConfiguration, 0, &xrViewConfigurationViewCount, nullptr);
    xrViewConfigurationViews = (XrViewConfigurationView*)malloc(sizeof(XrViewConfigurationView) * xrViewConfigurationViewCount);
    for (uint32_t i = 0; i < xrViewConfigurationViewCount; ++i)
    {
        xrViewConfigurationViews[i] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    }
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, xrViewConfiguration, xrViewConfigurationViewCount, &xrViewConfigurationViewCount, xrViewConfigurationViews);
}
void GfxCreateXRSwapchain()
{
    if (xrSession == XR_NULL_HANDLE || xrViewConfigurationViewCount == 0)
        return;

    uint32_t formatCount = 0;
    OPENXR_CHECK(xrEnumerateSwapchainFormats(xrSession, 0, &formatCount, nullptr), "xrEnumerateSwapchainFormats");
    int64_t * formats = (int64_t*)malloc(sizeof(int64_t) * formatCount);
    OPENXR_CHECK(xrEnumerateSwapchainFormats(xrSession, formatCount, &formatCount, formats), "xrEnumerateSwapchainFormats");
    free(formats);

    xrColorSwapchainInfos = (XrSwapchainInfo * ) malloc(sizeof(XrSwapchainInfo) * xrViewConfigurationViewCount);
    xrDepthSwapchainInfos = (XrSwapchainInfo * ) malloc(sizeof(XrSwapchainInfo) * xrViewConfigurationViewCount);
    for (size_t i = 0; i < xrViewConfigurationViewCount; i++) {
        XrSwapchainInfo * colorSwapchainInfo = &xrColorSwapchainInfos[i];
        XrSwapchainInfo * depthSwapchainInfo = &xrDepthSwapchainInfos[i];
        *colorSwapchainInfo = {};
        *depthSwapchainInfo = {};

        XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCreateInfo.createFlags = 0;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.format = (int64_t) VK_FORMAT_B8G8R8A8_SRGB;
        swapchainCreateInfo.sampleCount = xrViewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCreateInfo.width = xrViewConfigurationViews[i].recommendedImageRectWidth;
        swapchainCreateInfo.height = xrViewConfigurationViews[i].recommendedImageRectHeight;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.mipCount = 1;
        OPENXR_CHECK(xrCreateSwapchain(xrSession, &swapchainCreateInfo, &colorSwapchainInfo->swapchain), "xrCreateSwapchain(color)");
        colorSwapchainInfo->swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;

        swapchainCreateInfo.createFlags = 0;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        swapchainCreateInfo.format = (int64_t)GfxGetDepthFormat();
        swapchainCreateInfo.sampleCount = xrViewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCreateInfo.width = xrViewConfigurationViews[i].recommendedImageRectWidth;
        swapchainCreateInfo.height = xrViewConfigurationViews[i].recommendedImageRectHeight;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.mipCount = 1;
        OPENXR_CHECK(xrCreateSwapchain(xrSession, &swapchainCreateInfo, &depthSwapchainInfo->swapchain), "xrCreateSwapchain(depth)");
        depthSwapchainInfo->swapchainFormat = GfxGetDepthFormat();

        uint32_t colorSwapchainImageCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo->swapchain, 0, &colorSwapchainImageCount, nullptr), "xrEnumerateSwapchainImages(color count)");
        XrSwapchainImageVulkanKHR * colorSwapchainImages = (XrSwapchainImageVulkanKHR*)malloc(sizeof(XrSwapchainImageVulkanKHR) * colorSwapchainImageCount);
        for (uint32_t j = 0; j < colorSwapchainImageCount; ++j)
        {
            colorSwapchainImages[j] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR};
        }
        OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo->swapchain, colorSwapchainImageCount, &colorSwapchainImageCount, (XrSwapchainImageBaseHeader*) colorSwapchainImages), "xrEnumerateSwapchainImages(color)");

        uint32_t depthSwapchainImageCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo->swapchain, 0, &depthSwapchainImageCount, nullptr), "xrEnumerateSwapchainImages(depth count)");
        XrSwapchainImageVulkanKHR * depthSwapchainImages = (XrSwapchainImageVulkanKHR*)malloc(sizeof(XrSwapchainImageVulkanKHR) * depthSwapchainImageCount);
        for (uint32_t j = 0; j < depthSwapchainImageCount; ++j)
        {
            depthSwapchainImages[j] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR};
        }
        OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo->swapchain, depthSwapchainImageCount, &depthSwapchainImageCount, (XrSwapchainImageBaseHeader*) depthSwapchainImages), "xrEnumerateSwapchainImages(depth)");
        
        colorSwapchainInfo->imageViews = (VkImageView*)malloc(sizeof(VkImageView) * colorSwapchainImageCount);
        depthSwapchainInfo->imageViews = (VkImageView*)malloc(sizeof(VkImageView) * depthSwapchainImageCount);
        colorSwapchainInfo->images = (VkImage*)malloc(sizeof(VkImage) * colorSwapchainImageCount);
        depthSwapchainInfo->images = (VkImage*)malloc(sizeof(VkImage) * depthSwapchainImageCount);
        for (uint32_t j = 0; j < colorSwapchainImageCount; j++) {
            VkImageViewCreateInfo viewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewCreateInfo.image = colorSwapchainImages[j].image;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = colorSwapchainInfo->swapchainFormat;
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, &colorSwapchainInfo->imageViews[j]));
            colorSwapchainInfo->images[j] = colorSwapchainImages[j].image;
        }
        for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
            VkImageViewCreateInfo viewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewCreateInfo.image = depthSwapchainImages[j].image;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = depthSwapchainInfo->swapchainFormat;
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, &depthSwapchainInfo->imageViews[j]));
            depthSwapchainInfo->images[j] = depthSwapchainImages[j].image;

        }

        free(colorSwapchainImages);
        free(depthSwapchainImages);
    }
}

void GfxHandleOpenXREvents()
{
    switch (xrEventData.type)
    {
        case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            XrEventDataEventsLost * eventsLost = (XrEventDataEventsLost*) &xrEventData;
            spdlog::info("OPENXR: Events Lost: {}", eventsLost->lostEventCount);
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            XrEventDataInstanceLossPending  * instanceLossPending = (XrEventDataInstanceLossPending *) &xrEventData;
            spdlog::info("OPENXR: Instance Loss Pending at: {}", instanceLossPending->lossTime);
            xrSessionRunning = false;
            xrApplicationRunning = false;
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            XrEventDataInteractionProfileChanged *interactionProfileChanged = (XrEventDataInteractionProfileChanged *) &xrEventData;
            if (interactionProfileChanged->session != xrSession) 
            {
                spdlog::info("OPENXR: XR Interaction Profile Changed for Unknown Session");
            }
            break;
        }
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = (XrEventDataReferenceSpaceChangePending  *) &xrEventData;
            if (referenceSpaceChangePending->session != xrSession) 
            {
                spdlog::info("OPENXR: Reference Space Change Pending for Unknown Session");
            }
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            XrEventDataSessionStateChanged *sessionStateChanged = (XrEventDataSessionStateChanged *) &xrEventData;
            if (sessionStateChanged->session != xrSession) {
                spdlog::info("OPENXR: Session State Changed for Unknown Session");
                break;
            }
            if (sessionStateChanged->state == XR_SESSION_STATE_READY) 
            {
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = xrViewConfiguration;
                OPENXR_CHECK(xrBeginSession(xrSession, &sessionBeginInfo), "xrBeginSession");
                xrSessionRunning = true;
            }
            else if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) 
            {
                xrEndSession(xrSession);
                xrSessionRunning = false;
            }
            else if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) 
            {
                xrSessionRunning = false;
                xrApplicationRunning = false;
            }
            else if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) 
            {
                xrSessionRunning = false;
                xrApplicationRunning = false;
            }
            xrSessionState = sessionStateChanged->state;
            break;
        }
    }
}
#endif

glm::mat4 GfxGenerateModelMatrix(glm::vec3 position, glm::vec3 scale, glm::vec3 rotationEular)
{
    glm::mat4 model = glm::identity<glm::mat4>();
    model = glm::translate(model, position);

    model = glm::scale(model, scale);

    model = glm::rotate(model, glm::degrees(rotationEular.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::degrees(rotationEular.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::degrees(rotationEular.z), glm::vec3(0.0f, 0.0f, 1.0f));

    return model;
}

void GfxSceneAddRenderable(Scene &scene, RenderableCreateInfo *createInfo)
{
    if (scene.numRenderables >= MAX_RENDERABLES - 1)
    {
        return;
    }
    uint32_t index = scene.numRenderables;
    RenderableData data = {};

    Mesh *mesh = GetMesh(createInfo->meshID);

    for (uint32_t i = 0; i < 6; ++i)
    {
        data.textureIDs[i] = createInfo->textureIDs[i];
    }

    glm::mat4 transposed = glm::transpose(createInfo->transform);

    data.transformRow0 = transposed[0];
    data.transformRow1 = transposed[1];
    data.transformRow2 = transposed[2];

    data.mesh = createInfo->meshID;
    memcpy(&scene.pRenderableData[index], &data, sizeof(RenderableData));
    scene.numRenderables++;
}

extern Mesh meshRegistry[MAX_MESHES];
extern uint32_t meshRegistryCount;
void GfxSortRenderables(Scene &scene)
{
    VkDeviceAddress indirectDataAddress = GetBufferDeviceAddress(indirectDataBuffer, frameIndex);
    VkDeviceAddress renderableDataAddress = GetBufferDeviceAddress(renderableDataBuffer, frameIndex);
    VkDeviceAddress renderableIndexAddress = GetBufferDeviceAddress(renderableIndexBuffer, frameIndex);
    
    memcpy(GetBufferMappedAddress(indirectDataBuffer, frameIndex), &meshRegistry, GetNumMeshes() * sizeof(Mesh));
    memcpy(GetBufferMappedAddress(renderableDataBuffer, frameIndex), scene.pRenderableData, scene.numRenderables * sizeof(RenderableData));
    VkCommandBuffer cmd = GfxCreateOneTimeCmdBuffer();
    VkCommandBufferBeginInfo beginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));


    // Clear Specific values in indirect buffer
    vkCmdPushConstants(cmd, GetPipeline(pipelineClearIndirectCS)->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkDeviceAddress), &indirectDataAddress);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineClearIndirectCS)->pipeline);
    vkCmdDispatch(cmd, GetNumMeshes(), 1, 1);
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR};
    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier};

    // Increment indirect data based on number of renderables with each mesh
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIncrementIndirectCS)->pipeline);
    IncrementIndirectPushConstants incrementIndirectPushConstants =
    {
        .globalDataAddress = GetBufferDeviceAddress(globalDataBuffer, frameIndex),
        .indirectAddress = GetBufferDeviceAddress(indirectDataBuffer, frameIndex),
        .renderableDataAddress = GetBufferDeviceAddress(renderableDataBuffer, frameIndex),
        .numRenderables = scene.numRenderables
    };
    vkCmdPushConstants(cmd, GetPipeline(pipelineIncrementIndirectCS)->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IncrementIndirectPushConstants), &incrementIndirectPushConstants);
    vkCmdDispatch(cmd, (scene.numRenderables + 63) / 64, 1, 1);

    // Assign Mesh Offsets for each mesh
    uint32_t numMeshes = GetNumMeshes();
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineMeshOffsetsIndirectCS)->pipeline);

    MeshOffsetsPushConstants meshOffsetsPushConstants = 
    {
        .indirectAddress = indirectDataAddress,
        .numMeshes = numMeshes
    };

    vkCmdPushConstants(cmd, GetPipeline(pipelineMeshOffsetsIndirectCS)->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshOffsetsPushConstants), &meshOffsetsPushConstants);
    vkCmdDispatch(cmd, 1, 1, 1);

    // Calculate Renderable Indices
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIndicesIndirectCS)->pipeline);
    CalculateIndicesPushConstants calculateIndicesPushConstants =
    {
        .indirectAddress = indirectDataAddress,
        .renderableDataAddress = renderableDataAddress,
        .renderableIndexAddress = renderableIndexAddress,
        .numRenderables = scene.numRenderables
    };
    vkCmdPushConstants(cmd, GetPipeline(pipelineIndicesIndirectCS)->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CalculateIndicesPushConstants), &calculateIndicesPushConstants);
    vkCmdDispatch(cmd, (scene.numRenderables + 63) / 64, 1, 1);
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkQueue queue = GfxGetQueue();
    VkSubmitInfo submitInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd};
    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, true, UINT64_MAX);
}

void GfxSelectDevice()
{
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    if (deviceCount == 0)
    {
        spdlog::error("No Vulkan Capable Devices");
        abort();
    }
    VkPhysicalDevice *devices = (VkPhysicalDevice *)malloc(deviceCount * sizeof(VkPhysicalDevice));
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices));
    physicalDevice = devices[0];
    free(devices);
#ifdef USE_OPENXR
    if (xrInstance != XR_NULL_HANDLE && xrSystemID != XR_NULL_SYSTEM_ID)
    {
        PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
        if (XR_SUCCEEDED(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR)) && xrGetVulkanGraphicsDeviceKHR)
        {
            VkPhysicalDevice xrPhysicalDevice = VK_NULL_HANDLE;
            XrResult devRes = xrGetVulkanGraphicsDeviceKHR(xrInstance, xrSystemID, instance, &xrPhysicalDevice);
            if (XR_SUCCEEDED(devRes) && xrPhysicalDevice != VK_NULL_HANDLE)
            {
                physicalDevice = xrPhysicalDevice;
            }
            else
            {
                OPENXR_CHECK(devRes, "xrGetVulkanGraphicsDeviceKHR");
            }
        }
    }
#endif
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    heapProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT};
#endif
    VkPhysicalDeviceProperties2 deviceProperties =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
            .pNext = &heapProperties
#endif
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
    spdlog::info("Selected device: {}", deviceProperties.properties.deviceName);
}

void GfxCreateDevice()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties *)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);
    for (size_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
            glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, i))
        {
            queueFamily = i;
            break;
        }
    }
    const float qfProperties = 1.0f;

    VkDeviceQueueCreateInfo queueInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &qfProperties};
    std::vector<std::string> deviceExtensionStorage;
    auto addUniqueDeviceExt = [&](const char *name) {
        if (name && std::find(deviceExtensionStorage.begin(), deviceExtensionStorage.end(), name) == deviceExtensionStorage.end())
            deviceExtensionStorage.emplace_back(name);
    };
    addUniqueDeviceExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    addUniqueDeviceExt(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    addUniqueDeviceExt(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
#endif
#ifdef USE_OPENXR
    for (const auto &ext : xrVulkanDeviceExtensions)
    {
        addUniqueDeviceExt(ext.c_str());
    }
#endif
    std::vector<const char *> deviceExtensions;
    deviceExtensions.reserve(deviceExtensionStorage.size());
    for (const auto &ext : deviceExtensionStorage)
    {
        deviceExtensions.push_back(ext.c_str());
    }

    VkPhysicalDeviceVulkan11Features enabledVk11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = NULL, // End of the chain
        .storageBuffer16BitAccess = true,
        .uniformAndStorageBuffer16BitAccess = true,
    };
    VkPhysicalDeviceVulkan12Features enabledVk12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &enabledVk11Features,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageBufferUpdateAfterBind = true,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
        .bufferDeviceAddress = true,
        .bufferDeviceAddressCaptureReplay = true,
    };
    VkPhysicalDeviceVulkan13Features enabledVk13Features =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &enabledVk12Features,
            .synchronization2 = true,
            .dynamicRendering = true,
    };
    VkPhysicalDeviceFeatures enabledVk10Features =
        {
            .multiDrawIndirect = true,
            .samplerAnisotropy = true,
            .shaderInt16 = true,

    };

#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    VkPhysicalDeviceVulkan14Features enabledVk14Features =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
            .pNext = &enabledVk13Features};

    VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
            .pNext = &enabledVk14Features,
            .descriptorHeap = true,
            .descriptorHeapCaptureReplay = true};
#endif
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
        .pNext = &descriptorHeapFeatures,
#else
        .pNext = &enabledVk13Features,
#endif
        .features = enabledVk10Features};
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = NULL};
    spdlog::info("Number of enabled extensions: {}", deviceInfo.enabledExtensionCount);
    for (uint32_t i = 0; i < deviceInfo.enabledExtensionCount; ++i)
    {
        spdlog::info("Extension: {}", deviceInfo.ppEnabledExtensionNames[i]);
    }
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device));
    spdlog::info("Created Device");
    vkGetDeviceQueue(device, queueFamily, 0, &graphicsQueue);
    spdlog::info("Got device queue");
    volkLoadDevice(device);
    spdlog::info("Volk loaded device");
}

VkFormat GfxGetDepthFormat()
{
    VkFormat depthFormatList[] = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    for (uint32_t i = 0; i < 2; ++i)
    {
        VkFormatProperties2 formatProperties = {.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(physicalDevice, depthFormatList[i], &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = depthFormatList[i];
            break;
        }
    }
    return depthFormat;
}

void GfxCreateVMA()
{

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;

    vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

    VmaAllocatorCreateInfo allocatorInfo =
        {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &vulkanFunctions,
            .instance = instance,
            .vulkanApiVersion = VK_API_VERSION_1_3};

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));
    spdlog::info("Created VMA Allocator");
}

void GfxCreateSwapchain()
{
    #ifdef USE_OPENXR
        GfxCreateXRSwapchain();
        return;
    #endif

    VkExtent2D swapchainExtent = surfaceCaps.currentExtent;
    if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
    {
        swapchainExtent = {
            windowWidth,
            windowHeight};
    }
    const VkFormat imageFormat = {VK_FORMAT_B8G8R8A8_SRGB};
    VkSwapchainCreateInfoKHR swapchainCI = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = surfaceCaps.minImageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {.width = swapchainExtent.width, .height = swapchainExtent.height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR};
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, NULL, &swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL));
    swapchainImages = (VkImage *)malloc(swapchainImageCount * sizeof(VkImage));
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages));
    swapchainImageViews = (VkImageView *)malloc(swapchainImageCount * sizeof(VkImageView));

    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = imageFormat,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1}};
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]));
    }

    VkFormat depthFormat = GfxGetDepthFormat();

    VkImageCreateInfo depthImageCI = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent = {.width = windowWidth, .height = windowHeight, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocCI = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, NULL));
    vmaSetAllocationName(allocator, depthImageAllocation, "depth image");
    VkImageViewCreateInfo depthViewCI = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
    VK_CHECK(vkCreateImageView(device, &depthViewCI, NULL, &depthImageView));
}

void GfxDestroySwapchain()
{
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    free(swapchainImages);
    free(swapchainImageViews);
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    vmaDestroyImage(allocator, depthImage, depthImageAllocation);
    vkDestroyImageView(device, depthImageView, nullptr);
}

void GfxFramebufferResize(GLFWwindow *window, int width, int height)
{
    vkDeviceWaitIdle(device);

    windowWidth = (uint32_t)width;
    windowHeight = (uint32_t)height;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));
    GfxDestroySwapchain();
    GfxCreateSwapchain();
}

void GfxCreateWindow()
{
    glfwInit();
    glfwInitVulkanLoader(vkGetInstanceProcAddr);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(windowWidth, windowHeight, projectName, NULL, NULL);
    if (!window)
    {
        spdlog::error("Failed to create glfw window");
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, GfxFramebufferResize);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

void GfxCreateSurface()
{
    spdlog::info("About to Create Surface");
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    spdlog::info("Created Surface");
}

float GfxGetAspectRatio()
{
    return (float)windowWidth / (float)windowHeight;
}

void GfxCreateShaderData()
{
    BufferCreateInfo bufferInfo = {
        .debugName = "Global Data Buffer",
        .size = sizeof(GlobalData),
        .flags = BUFFER_FLAG_PER_FRAME | BUFFER_FLAG_MAPPED
    };
    globalDataBuffer = CreateBuffer(&bufferInfo);
}

void GfxCreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fences[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAcquiredSemaphores[i]));
    }
    renderCompleteSemaphores = (VkSemaphore *)malloc(sizeof(VkSemaphore) * swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]));
    }
}

void GfxCreateCommandBuffer()
{
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamily};
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = FRAMES_IN_FLIGHT};
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &commandBuffers[0]));
}



void GfxCreateComputePipelines()
{
    spdlog::log(spdlog::level::info, "Began Creating Compute Pipelines");
    
    PipelineLoadInfo pipelineInfo =
    {
        .shaderFile = "shaders/clearIndirectCS.spv",
        .descSetLayouts = nullptr,
        .numLayouts = 0,
        .flags = PIPELINE_FLAG_COMPUTE,
        .pushConstantSize = sizeof(VkDeviceAddress)
    };
    pipelineClearIndirectCS = LoadPipeline(&pipelineInfo);

    pipelineInfo.shaderFile = "shaders/incrementIndirectCS.spv";
    pipelineInfo.descSetLayouts = nullptr;
    pipelineInfo.numLayouts = 0;
    pipelineInfo.pushConstantSize = sizeof(IncrementIndirectPushConstants);
    pipelineIncrementIndirectCS = LoadPipeline(&pipelineInfo);

    pipelineInfo.shaderFile = "shaders/meshOffsetsIndirectCS.spv";
    pipelineInfo.descSetLayouts = nullptr;
    pipelineInfo.numLayouts = 0;
    pipelineInfo.pushConstantSize = sizeof(MeshOffsetsPushConstants);
    pipelineMeshOffsetsIndirectCS = LoadPipeline(&pipelineInfo);

    pipelineInfo.shaderFile = "shaders/calculateIndicesIndirectCS.spv";
    pipelineInfo.descSetLayouts = nullptr;
    pipelineInfo.numLayouts = 0;
    pipelineInfo.pushConstantSize = sizeof(CalculateIndicesPushConstants);
    pipelineIndicesIndirectCS = LoadPipeline(&pipelineInfo);
    spdlog::log(spdlog::level::info, "Created Compute Pipelines");
}

VkCommandPool GfxGetCommandPool()
{
    return commandPool;
}

VkQueue GfxGetQueue()
{
    return graphicsQueue;
}
void GfxImguiNewFrame()
{
    #ifndef USE_OPENXR
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    #endif
}
void GfxCreateDescriptorPool()
{
    VkDescriptorPoolSize poolSizes[] =
        {
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .descriptorCount = MAX_TEXTURES},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             .descriptorCount = MAX_RENDERABLES * 8},
    };
    VkDescriptorPoolCreateInfo descPoolCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = MAX_DESCRIPTOR_SETS * FRAMES_IN_FLIGHT,
            .poolSizeCount = 2,
            .pPoolSizes = poolSizes};
    VK_CHECK(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &descriptorPool));

    VkDescriptorPoolSize imguiPoolSizes[11] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64},
    };
    descPoolCreateInfo.poolSizeCount = 11;
    descPoolCreateInfo.pPoolSizes = imguiPoolSizes;
    VK_CHECK(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &imguiDescriptorPool));
}
VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    if (alignment == 0)
    {
        return value;
    }
    return (value + alignment - 1) / alignment * alignment;
}
VkDeviceSize addDescriptorToHeap(VkDeviceSize &cursor, VkDeviceSize size, VkDeviceSize alignment)
{
    cursor = alignUp(cursor, alignment);
    VkDeviceSize offset = cursor;
    cursor += size;
    return offset;
}
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
void GfxCreateDescriptorHeap()
{
    // Descriptor Heap Layout
    /*
        [reserved]
        [PerFrame 0]
        [PerFrame 1]
        [Textures]
    */
    VkDeviceSize heapCursor = heapProperties.minResourceHeapReservedRange;
    heapData.alignment = glm::max(glm::max(heapProperties.bufferDescriptorAlignment,
                                           heapProperties.samplerDescriptorAlignment),
                                  heapProperties.imageDescriptorAlignment);
    VkDeviceSize ubDescSize = vkGetPhysicalDeviceDescriptorSizeEXT(physicalDevice, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDeviceSize imageDescSize = vkGetPhysicalDeviceDescriptorSizeEXT(physicalDevice, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    VkDeviceSize samplerDescSize = vkGetPhysicalDeviceDescriptorSizeEXT(physicalDevice, VK_DESCRIPTOR_TYPE_SAMPLER);
    heapData.perFrameOffset = addDescriptorToHeap(
        heapCursor,
        ubDescSize,
        heapData.alignment);
    addDescriptorToHeap(
        heapCursor,
        ubDescSize,
        heapData.alignment);
    heapData.textureOffset = addDescriptorToHeap(
        heapCursor,
        imageDescSize * MAX_TEXTURES,
        heapData.alignment);
    heapData.resourceHeapSize = heapCursor;

    heapCursor = heapProperties.minResourceHeapReservedRange;
    heapData.samplerLinearOffset = addDescriptorToHeap(
        heapCursor,
        samplerDescSize,
        heapData.alignment);
    heapData.samplerNearestOffset = addDescriptorToHeap(
        heapCursor,
        samplerDescSize,
        heapData.alignment);

    heapData.samplerHeapSize = heapCursor;

    VkBufferCreateInfo heapBufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = heapData.resourceHeapSize,
            .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR};

    VmaAllocationCreateInfo heapBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateBuffer(allocator, &heapBufferInfo, &heapBufferAllocCreateInfo, &heapData.resourceHeapBuffer, &heapData.resourceHeapAllocation, &heapData.resourceHeapAllocInfo));
    vmaSetAllocationName(allocator, heapData.resourceHeapAllocation, "Resource Heap Buffer");

    VkBufferDeviceAddressInfo bdaInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = heapData.resourceHeapBuffer};
    heapData.resourceHeapDeviceAddress = vkGetBufferDeviceAddress(device, &bdaInfo);

    VkDeviceAddressRangeEXT deviceAddrRanges[2] = {
        {.address = globalDataBuffers[0].deviceAddress,
         .size = sizeof(ShaderData)},
        {.address = globalDataBuffers[1].deviceAddress,
         .size = sizeof(ShaderData)}};
    VkResourceDescriptorInfoEXT resourceDescriptorInfos[2] = {
        // Per Frame 0
        {
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .data = {
                .pAddressRange = &deviceAddrRanges[0]}},
        // Per Frame 1
        {.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .data = {.pAddressRange = &deviceAddrRanges[1]}}

    };
    VkHostAddressRangeEXT addrRanges[2] = {
        {(uint8_t *)heapData.resourceHeapAllocInfo.pMappedData + heapData.perFrameOffset, ubDescSize},
        {(uint8_t *)heapData.resourceHeapAllocInfo.pMappedData + heapData.perFrameOffset + heapData.alignment, ubDescSize}};
    vkWriteResourceDescriptorsEXT(device, 2, resourceDescriptorInfos, addrRanges);

    VkBufferCreateInfo samplerHeapBufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = heapData.samplerHeapSize,
            .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR};

    VmaAllocationCreateInfo samplerHeapBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VK_CHECK(vmaCreateBuffer(allocator, &samplerHeapBufferInfo, &samplerHeapBufferAllocCreateInfo, &heapData.samplerHeapBuffer, &heapData.samplerHeapAllocation, &heapData.samplerHeapAllocInfo));
    vmaSetAllocationName(allocator, heapData.samplerHeapAllocation, "Sampler Heap Buffer");

    bdaInfo.buffer = heapData.samplerHeapBuffer;
    heapData.samplerHeapDeviceAddress = vkGetBufferDeviceAddress(device, &bdaInfo);

    VkSamplerCreateInfo samplerInfos[] = {
        {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
         .magFilter = VK_FILTER_LINEAR,
         .minFilter = VK_FILTER_LINEAR,
         .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
         .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .maxLod = VK_LOD_CLAMP_NONE},
        {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
         .magFilter = VK_FILTER_NEAREST,
         .minFilter = VK_FILTER_NEAREST,
         .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
         .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
         .maxLod = VK_LOD_CLAMP_NONE}};

    VkHostAddressRangeEXT samplerRanges[] =
        {
            {.address = (uint8_t *)heapData.samplerHeapAllocInfo.pMappedData + heapData.samplerLinearOffset,
             .size = samplerDescSize},
            {.address = (uint8_t *)heapData.samplerHeapAllocInfo.pMappedData + heapData.samplerNearestOffset,
             .size = samplerDescSize}};
    vkWriteSamplerDescriptorsEXT(device, 2, samplerInfos, samplerRanges);

    heapData.bindingMappings[0] =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
            .descriptorSet = 0,
            .firstBinding = 0,
            .bindingCount = 1,
            .resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT,
            .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
            .sourceData =
                {
                    .pushIndex =
                        {
                            .heapOffset = heapData.perFrameOffset,
                            .pushOffset = 0,
                            .heapIndexStride = heapData.alignment}},
    };

    heapData.bindingMappings[1] =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
            .descriptorSet = 0,
            .firstBinding = 1,
            .bindingCount = 1,
            .resourceMask = VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT,
            .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
            .sourceData =
                {
                    .constantOffset =
                        {
                            .heapOffset = heapData.textureOffset,
                            .samplerHeapOffset = heapData.samplerNearestOffset}}};
}

void GfxGetBindingMappings(VkDescriptorSetAndBindingMappingEXT **bindingMappings, uint32_t *bindingCount)
{
    (*bindingMappings) = heapData.bindingMappings;
    (*bindingCount) = 2;
}
void GfxUpdateTextureHeapDescriptors(Texture *textures, uint32_t count, uint32_t offset)
{
    VkImageViewCreateInfo texImgViewInfos[MAX_TEXTURES];
    VkImageDescriptorInfoEXT imageInfos[MAX_TEXTURES];
    VkHostAddressRangeEXT hostAddrRanges[MAX_TEXTURES];
    VkResourceDescriptorInfoEXT resourceDescriptorInfos[MAX_TEXTURES];
    for (uint32_t i = 0; i < count; ++i)
    {
        texImgViewInfos[i] =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = textures[i].image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = textures[i].format,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = textures[i].layerCount}};
        imageInfos[i].pNext = NULL;
        imageInfos[i].sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT;
        imageInfos[i].pView = &texImgViewInfos[i];
        imageInfos[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        hostAddrRanges[i].address = (uint8_t *)heapData.resourceHeapAllocInfo.pMappedData + heapData.textureOffset + heapData.alignment * (i + offset);
        hostAddrRanges[i].size = heapData.alignment;
        resourceDescriptorInfos[i] =
            {
                .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .data = {
                    .pImage = &imageInfos[i]}};
    }
    vkWriteResourceDescriptorsEXT(device, count, resourceDescriptorInfos, hostAddrRanges);
}
#endif

void GfxUpdateTextures(Texture *textures, uint32_t count, uint32_t offset)
{
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    GfxUpdateTextureHeapDescriptors(textures, count, offset);
    return;
#endif
    VkDescriptorImageInfo textureDescriptors[MAX_TEXTURES];
    for (uint32_t i = 0; i < count; ++i)
    {
        textureDescriptors[i].sampler = textures[i].sampler;
        textureDescriptors[i].imageLayout = textures[i].imageLayout;
        textureDescriptors[i].imageView = textures[i].imageView;
    }
    VkWriteDescriptorSet writeDescSet =
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSetTex,
            .dstBinding = 0,
            .dstArrayElement = offset,
            .descriptorCount = count,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = textureDescriptors};
    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
}
void GfxCreateScene(Scene *pScene)
{
    pScene->pRenderableData = (RenderableData *)malloc(sizeof(RenderableData) * MAX_RENDERABLES);
    pScene->numRenderables = 0;
}
void GfxUpdateScene(Scene *pScene)
{
    renderableCount = pScene->numRenderables;
}

void GfxSetCameraData(glm::mat4 &cameraView, glm::vec3 cameraPos, float fov, float nearPlane, float farPlane, bool updateFrustum)
{
    GlobalData globalData;
    globalData.proj = glm::perspective(glm::radians(fov), GfxGetAspectRatio(), nearPlane, farPlane);
    globalData.proj[1][1] *= -1;
    globalData.view = cameraView;
    globalData.cameraPos = glm::vec4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);

    // Calculate frustum planes
    glm::mat4 vpt = glm::transpose(globalData.proj * globalData.view);
    if (updateFrustum)
    {
        frustumPlanes[0] = (vpt[3] + vpt[0]); // left
        frustumPlanes[1] = (vpt[3] - vpt[0]); // right
        frustumPlanes[2] = (vpt[3] + vpt[1]); // bottom
        frustumPlanes[3] = (vpt[3] - vpt[1]); // top
        frustumPlanes[4] = (vpt[3] + vpt[2]); // near
        frustumPlanes[5] = (vpt[3] - vpt[2]); // far
    }

    memcpy(globalData.frustumPlanes, frustumPlanes, sizeof(glm::vec4) * 6);

    memcpy(GetBufferMappedAddress(globalDataBuffer, frameIndex), &globalData, sizeof(GlobalData));
}

void GfxCreateImGui()
{
    #ifndef USE_OPENXR
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = swapchainImageCount;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.UseDynamicRendering = true;
    VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = GfxGetDepthFormat();
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo);
    #endif
}

void GfxInit()
{
    GfxCreateWindow();
    #ifdef USE_OPENXR
    GfxCreateOpenXRInstance();
    #endif
    GfxCreateInstance();
    GfxSetupDebugMessanger();
    GfxSelectDevice();
    GfxCreateDevice();
    #ifdef USE_OPENXR
    GfxCreateOpenXRSession();
    #endif
    GfxCreateSurface();
    GfxCreateVMA();
    GfxCreateSwapchain();
    GfxCreateSyncObjects();
    GfxCreateCommandBuffer();
    AssetLoaderInit();
    GfxCreateShaderData();
    GfxCreateDescriptorPool();
    GfxCreateImGui();
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    GfxCreateDescriptorHeap();
#endif
    GfxCreateBindlessRenderableBuffer();
    GfxCreateRenderableIndexBuffer();
    GfxCreateIndirectBuffer();



    GfxCreateBindlessDesc();
    GfxCreateComputePipelines();
}


int GfxUpdate()
{
    #ifdef USE_OPENXR
        if (xrInstance != XR_NULL_HANDLE)
        {
            while (true)
            {
                xrEventData = {XR_TYPE_EVENT_DATA_BUFFER};
                XrResult pollRes = xrPollEvent(xrInstance, &xrEventData);
                if (pollRes == XR_SUCCESS)
                {
                    GfxHandleOpenXREvents();
                }
                else if (pollRes == XR_EVENT_UNAVAILABLE)
                {
                    break;
                }
                else
                {
                    OPENXR_CHECK(pollRes, "xrPollEvent");
                    return 0;
                }
            }
            if (!xrApplicationRunning) return 0;
        }
    #endif

    glfwPollEvents();
    glfwSwapBuffers(window);

    return !glfwWindowShouldClose(window);
}

void GfxSetViewportAndScissors(VkCommandBuffer &cmd, uint32_t width, uint32_t height)
{
    VkViewport viewport =
        {
            .width = (float)width,
            .height = (float)height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
    VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {
                width,
                height}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
void GfxBindDescriptorHeaps()
{
    VkBindHeapInfoEXT bindHeapInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
            .heapRange = {
                .address = heapData.resourceHeapDeviceAddress,
                .size = heapData.resourceHeapSize},
            .reservedRangeOffset = 0,
            .reservedRangeSize = heapProperties.minResourceHeapReservedRange};
    vkCmdBindResourceHeapEXT(cmd, &bindHeapInfo);

    bindHeapInfo.heapRange =
        {
            .address = heapData.samplerHeapDeviceAddress,
            .size = heapData.samplerHeapSize};
    vkCmdBindSamplerHeapEXT(cmd, &bindHeapInfo);

    PushConstants pushData = {frameIndex};

    VkPushDataInfoEXT pushDataInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
            .offset = 0,
            .data =
                {
                    .address = &pushData,
                    .size = sizeof(PushConstants)}};

    vkCmdPushDataEXT(cmd, &pushDataInfo);
}
#endif

void GfxRenderScene(VkCommandBuffer &cmd)
{
    GfxSetViewportAndScissors(cmd, windowWidth, windowHeight);

#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    GfxBindDescriptorHeaps();
#else
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 1, &descriptorSetTex, 0, nullptr);

    MatGenericPushConstants matPushConstants =
    {
        .globalDataAddress = GetBufferDeviceAddress(globalDataBuffer, frameIndex),
        .renderableDataAddress = GetBufferDeviceAddress(renderableDataBuffer, frameIndex),
        .renderableIndexAddress = GetBufferDeviceAddress(renderableIndexBuffer, frameIndex)
    };
    vkCmdPushConstants(cmd, pipeline->layout, VK_SHADER_STAGE_ALL, 0, sizeof(MatGenericPushConstants), &matPushConstants);
#endif

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    VkDeviceSize vOffset{0};
    VkBuffer vertBuffer = GetMeshVertexBuffer();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vOffset);
    vkCmdBindIndexBuffer(cmd, GetMeshIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(cmd, GetBufferVK(indirectDataBuffer, frameIndex), 0, (uint32_t)GetNumMeshes(), sizeof(Mesh));
}

void GfxSetRenderableData(Scene *pScene)
{
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        memcpy(GetBufferMappedAddress(renderableDataBuffer, i), pScene->pRenderableData, sizeof(RenderableData) * pScene->numRenderables);
    }
}
void GfxWaitForFrame()
{
    VK_CHECK(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
    #ifndef USE_OPENXR
    VK_CHECK(vkResetFences(device, 1, &fences[frameIndex]));
    #endif
}
int GfxRender()
{
    #ifdef USE_OPENXR
        if (!xrSessionRunning) return 0;
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        OPENXR_CHECK(xrWaitFrame(xrSession, &frameWaitInfo, &frameState), "xrWaitFrame");

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        OPENXR_CHECK(xrBeginFrame(xrSession, &frameBeginInfo), "xrBeginFrame");
        bool rendered = false;
        RenderLayerInfo renderLayerInfo;
        renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

        bool sessionActive = (xrSessionState == XR_SESSION_STATE_SYNCHRONIZED || xrSessionState == XR_SESSION_STATE_VISIBLE || xrSessionState == XR_SESSION_STATE_FOCUSED);
        if (!(sessionActive && frameState.shouldRender)) {
            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            frameEndInfo.layerCount = 0;
            frameEndInfo.layers = nullptr;
            OPENXR_CHECK(xrEndFrame(xrSession, &frameEndInfo), "xrEndFrame");
            return 0;
        }

        std::vector<XrView> views(xrViewConfigurationViewCount, {XR_TYPE_VIEW});
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = xrViewConfiguration;
        viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
        viewLocateInfo.space = xrLocalSpace;
        uint32_t viewCount = 0;
        OPENXR_CHECK(xrLocateViews(xrSession, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data()), "xrLocateViews");
    #else
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);
    #endif

    #ifdef USE_OPENXR
        std::vector<XrCompositionLayerProjectionView> projectionViews(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
        renderLayerInfo.layerProjectionViews = projectionViews.data();
        for (uint32_t i = 0; i < viewCount; i++) 
        {
            XrSwapchainInfo &colorSwapchainInfo = xrColorSwapchainInfos[i];
            XrSwapchainInfo &depthSwapchainInfo = xrDepthSwapchainInfos[i];
            uint32_t colorImageIndex = 0;
            uint32_t depthImageIndex = 0;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            OPENXR_CHECK(xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex), "xrAcquireSwapchainImage(color)");
            OPENXR_CHECK(xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex), "xrAcquireSwapchainImage(depth)");
            XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo), "xrWaitSwapchainImage(color)");
            OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo), "xrWaitSwapchainImage(depth)");
            const uint32_t &width = xrViewConfigurationViews[i].recommendedImageRectWidth;
            const uint32_t &height = xrViewConfigurationViews[i].recommendedImageRectHeight;

            renderLayerInfo.layerProjectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
            renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
            renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
            renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = 0;

            VkCommandBuffer cmd = commandBuffers[frameIndex];
            VkCommandBufferBeginInfo beginInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
            VK_CHECK(vkResetCommandBuffer(cmd, 0));
            VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

            VkImageMemoryBarrier2 outputBarriers[] =
                {
                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                     .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     .srcAccessMask = 0,
                     .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                     .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                     .image = colorSwapchainInfo.images[colorImageIndex],
                     .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}},
                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                     .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                     .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                     .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                     .image = depthSwapchainInfo.images[depthImageIndex],
                     .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1}}};
            VkDependencyInfo barrierDependencyInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = 2,
                    .pImageMemoryBarriers = outputBarriers};
            vkCmdPipelineBarrier2(cmd, &barrierDependencyInfo);

            VkRenderingAttachmentInfo colorAttachmentInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = colorSwapchainInfo.imageViews[colorImageIndex],
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {0.0f, 0.5f, 1.0f, 1.0f}}};
            VkRenderingAttachmentInfo depthAttachmentInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = depthSwapchainInfo.imageViews[depthImageIndex],
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = {.depthStencil = {1.0f, 0}}};

            VkRenderingInfo renderingInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea =
                        {
                            .extent = {
                                .width = width,
                                .height = height}},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &colorAttachmentInfo,
                    .pDepthAttachment = &depthAttachmentInfo};
            vkCmdBeginRendering(cmd, &renderingInfo);
            vkCmdEndRendering(cmd);

            VkImageMemoryBarrier2 barrierPresent{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = colorSwapchainInfo.images[colorImageIndex],
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
            VkDependencyInfo barrierPresentDependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrierPresent};
            vkCmdPipelineBarrier2(cmd, &barrierPresentDependencyInfo);

            VK_CHECK(vkEndCommandBuffer(cmd));

            VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = nullptr,
                .pWaitDstStageMask = nullptr,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = nullptr};
            VK_CHECK(vkResetFences(device, 1, &fences[frameIndex]));
            VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[frameIndex]));
            if (i + 1 < viewCount)
            {
                VK_CHECK(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
            }

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo), "xrReleaseSwapchainImage(color)");
            OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo), "xrReleaseSwapchainImage(depth)");
        }
        renderLayerInfo.layerProjection.space = xrLocalSpace;
        renderLayerInfo.layerProjection.viewCount = viewCount;
        renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews;
    #endif

    #ifndef USE_OPENXR
    VkCommandBuffer cmd = commandBuffers[frameIndex];
    VkCommandBufferBeginInfo beginInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier2 outputBarriers[] =
        {
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .srcAccessMask = 0,
             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = swapchainImages[imageIndex],
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
             .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = depthImage,
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1}}};
    VkDependencyInfo barrierDependencyInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = outputBarriers};
    vkCmdPipelineBarrier2(cmd, &barrierDependencyInfo);

    VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainImageViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {0.0f, 0.5f, 1.0f, 1.0f}}};
    VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthImageView,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {.depthStencil = {1.0f, 0}}};

    VkExtent2D extent =
        {
            .width = windowWidth,
            .height = windowHeight};
    VkRenderingInfo renderingInfo =
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .extent = {
                        .width = windowWidth,
                        .height = windowHeight}},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo};
    vkCmdBeginRendering(cmd, &renderingInfo);

    GfxRenderScene(cmd);

    #ifndef USE_OPENXR
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    #endif
    vkCmdEndRendering(cmd);

    VkImageMemoryBarrier2 barrierPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainImages[imageIndex],
        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
    VkDependencyInfo barrierPresentDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierPresent};
    vkCmdPipelineBarrier2(cmd, &barrierPresentDependencyInfo);

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAcquiredSemaphores[frameIndex],
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderCompleteSemaphores[imageIndex],
    };
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[frameIndex]));
    #endif

    frameIndex = (frameIndex + 1) % FRAMES_IN_FLIGHT;
    #ifndef USE_OPENXR
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex};
    vkQueuePresentKHR(graphicsQueue, &presentInfo);
    #else 
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layerCount = 1;
        XrCompositionLayerBaseHeader * layer = (XrCompositionLayerBaseHeader *)&renderLayerInfo.layerProjection;
        frameEndInfo.layers = &layer;
        OPENXR_CHECK(xrEndFrame(xrSession, &frameEndInfo), "xrEndFrame");
    #endif
    return 0;
}

void GfxPreDestroy()
{
    VK_CHECK(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &fences[frameIndex]));

    vkDeviceWaitIdle(device);
    vkQueueWaitIdle(graphicsQueue);
}

void GfxDestroy()
{
    vkDeviceWaitIdle(device);
    #ifndef USE_OPENXR
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    #endif

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroyFence(device, fences[i], nullptr);
        vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
    }
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
    }

    free(renderCompleteSemaphores);

    GfxDestroySwapchain();
    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTex, nullptr);

    vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSetTex);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);

    char *statsString;
    vmaBuildStatsString(allocator, &statsString, 1);

    FILE *statsFile = fopen("vmaStats.json", "w");

    fputs(statsString, statsFile);

    fclose(statsFile);

    vmaFreeStatsString(allocator, statsString);

    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
}

GLFWwindow *GfxGetWindow()
{
    return window;
}

void GfxCreateRenderableIndexBuffer()
{
    BufferCreateInfo bufferInfo = {
        .debugName = "Renderable Index Buffer",
        .size = sizeof(uint32_t) * MAX_RENDERABLES,
        .flags = BUFFER_FLAG_PER_FRAME
    };
    renderableIndexBuffer = CreateBuffer(&bufferInfo);
}

void GfxCreateBindlessRenderableBuffer()
{
    BufferCreateInfo bufferInfo = {
        .debugName = "Renderable Data Buffer",
        .size = sizeof(RenderableData) * MAX_RENDERABLES,
        .flags = BUFFER_FLAG_PER_FRAME | BUFFER_FLAG_MAPPED
    };
    renderableDataBuffer = CreateBuffer(&bufferInfo);
}

void GfxCreateIndirectBuffer()
{
    BufferCreateInfo bufferInfo = {
        .debugName = "Indirect Buffer",
        .size = sizeof(Mesh) * MAX_MESHES,
        .flags = BUFFER_FLAG_INDIRECT_BUFFER | BUFFER_FLAG_PER_FRAME | BUFFER_FLAG_MAPPED
    };
    indirectDataBuffer = CreateBuffer(&bufferInfo);
}

void GfxCreateBindlessDesc()
{
    // Create the Descriptor Layout
    VkDescriptorBindingFlags descVariableFlags[] =
        {
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 1,
            .pBindingFlags = descVariableFlags};

    // Texture Descriptor Set
    VkDescriptorSetLayoutBinding descLayoutBindingTex = {
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo descLayoutTexCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &descLayoutBindingTex,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descLayoutTexCreateInfo, NULL, &descriptorSetLayoutTex));

    uint32_t variableDescCount = MAX_TEXTURES;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &variableDescCount};
    VkDescriptorSetAllocateInfo texDescSetAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableDescCountAllocInfo,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayoutTex};
    VK_CHECK(vkAllocateDescriptorSets(device, &texDescSetAllocInfo, &descriptorSetTex));

    // Renderable Data/Indices Descriptor Set and Layout
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        // ...
    }
}

VkCommandBuffer GfxCreateOneTimeCmdBuffer()
{
    VkCommandBufferAllocateInfo cmdAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = GfxGetCommandPool(),
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1};
    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmdBuffer);
    return cmdBuffer;
}

VkDescriptorSetLayout GfxGetDescriptorSetLayoutTextures()
{
    return descriptorSetLayoutTex;
}
