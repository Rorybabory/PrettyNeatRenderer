
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
#include "vulkan_renderer.h"
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vkutils.h"

#include "asset_loader.h"

#define WINDOW_BASE_WIDTH 1280
#define WINDOW_BASE_HEIGHT 720

#define MAX_TEXTURES 512

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

// Sync Objects
VkSemaphore imageAcquiredSemaphores[FRAMES_IN_FLIGHT];
VkSemaphore *renderCompleteSemaphores;
VkFence fences[FRAMES_IN_FLIGHT];

Pipeline *pipeline = NULL;
Pipeline *pipelineLayout = NULL;
Mesh *mesh = NULL;

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

VkDescriptorSetLayout descriptorSetLayoutTex;
VkDescriptorSet descriptorSetTex;

VkDescriptorSetLayout descriptorSetLayoutRenderables;
VkDescriptorSet descriptorSetsRenderableData[FRAMES_IN_FLIGHT];

VkDescriptorSetLayout descriptorSetLayoutRenderableIndices;
VkDescriptorSet descriptorSetsRenderableIndices[FRAMES_IN_FLIGHT];
uint32_t renderableIndices[MAX_RENDERABLES];

VkDescriptorSetLayout descriptorSetLayoutIndirect;
VkDescriptorSet descriptorSetsIndirect[FRAMES_IN_FLIGHT];

VkBuffer renderableDataBuffers[FRAMES_IN_FLIGHT];
VmaAllocation renderableDataAllocations[FRAMES_IN_FLIGHT];
VmaAllocationInfo renderableDataAllocInfo[FRAMES_IN_FLIGHT];

VkBuffer indirectDataBuffers[FRAMES_IN_FLIGHT];
VmaAllocation indirectDataAllocations[FRAMES_IN_FLIGHT];
VmaAllocationInfo indirectDataAllocInfo[FRAMES_IN_FLIGHT];

VkBuffer renderableIndexBuffers[FRAMES_IN_FLIGHT];
VmaAllocation renderableIndexAllocations[FRAMES_IN_FLIGHT];
VmaAllocationInfo renderableIndexAllocInfo[FRAMES_IN_FLIGHT];

VkDrawIndexedIndirectCommand renderableIndirectCommands[MAX_MESHES];


uint32_t renderableCount = 0;

PipelineID pipelineClearIndirectCS;
PipelineID pipelineIncrementIndirectCS;
PipelineID pipelineMeshOffsetsIndirectCS;
PipelineID pipelineIndicesIndirectCS;

#endif

struct GlobalDataBuffer
{
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo allocInfo;
    VkDeviceAddress deviceAddress;
};

GlobalDataBuffer globalDataBuffers[FRAMES_IN_FLIGHT];

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
void GfxSetMesh(Mesh *m)
{
    mesh = m;
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
    uint32_t instanceExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCountGlfw);
    instanceExtensionCount = instanceExtensionCountGlfw + 1;
    char **instanceExtension = (char **)malloc(sizeof(char *) * instanceExtensionCount);

    for (uint32_t i = 0; i < instanceExtensionCount; ++i)
    {
        instanceExtension[i] = (char *)malloc(sizeof(char) * 128);
    }

    for (uint32_t i = 0; i < instanceExtensionCountGlfw; ++i)
    {
        strcpy(instanceExtension[i], glfwExtensions[i]);
    }
    strcpy(instanceExtension[instanceExtensionCountGlfw], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = instanceExtensionCount;
    createInfo.ppEnabledExtensionNames = (const char *const *)instanceExtension;

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

    for (uint32_t i = 0; i < instanceExtensionCount; ++i)
    {
        free(instanceExtension[i]);
    }
    free(instanceExtension);
    volkLoadInstance(instance);
}

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
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        memcpy(indirectDataAllocInfo[i].pMappedData, &meshRegistry, GetNumMeshes() * sizeof(Mesh));
        memcpy(renderableDataAllocInfo[i].pMappedData, scene.pRenderableData, scene.numRenderables * sizeof(RenderableData));
    }
    spdlog::log(spdlog::level::info, "Began Sorting Renderables");
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VkCommandBuffer cmd = GfxCreateOneTimeCmdBuffer();
        VkCommandBufferBeginInfo beginInfo =
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
        spdlog::log(spdlog::level::info, "Started recording sorting command buffer");
        //Clear Specific values in indirect buffer
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineClearIndirectCS)->layout, 0, 1, &descriptorSetsIndirect[i], 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineClearIndirectCS)->pipeline);
        vkCmdDispatch(cmd, GetNumMeshes(), 1, 1);
        spdlog::log(spdlog::level::info, "recorded clear command sorting command buffer");
        VkMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR
        };
        VkDependencyInfo dependencyInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        };

        //Increment indirect data based on number of renderables with each mesh
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIncrementIndirectCS)->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIncrementIndirectCS)->layout, 0, 1, &descriptorSetsIndirect[i], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIncrementIndirectCS)->layout, 1, 1, &descriptorSetsRenderableData[i], 0, nullptr);
        vkCmdDispatch(cmd, scene.numRenderables, 1, 1);
        spdlog::log(spdlog::level::info, "recorded increment indirect sorting command buffer");

        //Assign Mesh Offsets for each mesh
        uint32_t numMeshes = GetNumMeshes();
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineMeshOffsetsIndirectCS)->layout, 0, 1, &descriptorSetsIndirect[i], 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineMeshOffsetsIndirectCS)->pipeline);
        VkPushConstantsInfo pushInfo = {
            .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
            .layout = GetPipeline(pipelineMeshOffsetsIndirectCS)->layout,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .size = sizeof(uint32_t),
            .pValues = &numMeshes,
        };
        vkCmdPushConstants(cmd,GetPipeline(pipelineMeshOffsetsIndirectCS)->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &numMeshes);
        vkCmdDispatch(cmd, 1, 1, 1);
        spdlog::log(spdlog::level::info, "recorded assign mesh offsets sorting command buffer");


        //Calculate Renderable Indices
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIndicesIndirectCS)->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIndicesIndirectCS)->layout, 0, 1, &descriptorSetsRenderableIndices[i], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIndicesIndirectCS)->layout, 1, 1, &descriptorSetsRenderableData[i], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipeline(pipelineIndicesIndirectCS)->layout, 2, 1, &descriptorSetsIndirect[i], 0, nullptr);
        vkCmdDispatch(cmd, scene.numRenderables, 1, 1);
        spdlog::log(spdlog::level::info, "recorded renderable indices sorting command buffer");
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);

        VK_CHECK(vkEndCommandBuffer(cmd));
        spdlog::log(spdlog::level::info, "Finished recording sorting command buffer");
        VkQueue queue = GfxGetQueue();
        VkSubmitInfo submitInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
        };
        VkFence fence;
        VkFenceCreateInfo fenceCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
        spdlog::log(spdlog::level::info, "Submitted renderable sorting command buffers");
        vkQueueSubmit(queue, 1, &submitInfo, fence);

        vkWaitForFences(device, 1, &fence, true, UINT64_MAX);
    }
    spdlog::log(spdlog::level::info, "Finished Sorting Renderables");
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
    const char *deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,

#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
        VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME
#endif
    };

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
#ifndef DESCRIPTOR_HEAP_EXPERIMENTS
        .enabledExtensionCount = 1,
#else
        .enabledExtensionCount = 3,
#endif

        .ppEnabledExtensionNames = &deviceExtensions[0],
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
    VkBufferCreateInfo bufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(GlobalData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(
            vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocCreateInfo,
                &globalDataBuffers[i].buffer,
                &globalDataBuffers[i].alloc,
                &globalDataBuffers[i].allocInfo));
        VkBufferDeviceAddressInfo bdaInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = globalDataBuffers[i].buffer};
        globalDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &bdaInfo);

        char allocName[96];
        sprintf(allocName, "Global Data Allocation: %i", i);

        vmaSetAllocationName(allocator, globalDataBuffers[i].alloc, allocName);
    }
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
    VkDescriptorSetLayout layoutClear[] =
    {
        descriptorSetLayoutIndirect
    };
    PipelineLoadInfo pipelineInfo = 
    {
        .shaderFile = "shaders/clearIndirectCS.spv",
        .descSetLayouts = layoutClear,
        .numLayouts = 1,
        .flags = PIPELINE_FLAG_COMPUTE
    };
    pipelineClearIndirectCS = LoadPipeline(&pipelineInfo);


    VkDescriptorSetLayout layoutInc[] =
    {
        descriptorSetLayoutIndirect,
        descriptorSetLayoutRenderables
    };
    pipelineInfo.shaderFile = "shaders/incrementIndirectCS.spv";
    pipelineInfo.descSetLayouts = layoutInc;
    pipelineInfo.numLayouts = 2;
    pipelineIncrementIndirectCS = LoadPipeline(&pipelineInfo);


    VkDescriptorSetLayout layoutMeshOffsets[] =
    {
        descriptorSetLayoutIndirect
    };
    pipelineInfo.shaderFile = "shaders/meshOffsetsIndirectCS.spv";
    pipelineInfo.descSetLayouts = layoutMeshOffsets;
    pipelineInfo.numLayouts = 1;
    pipelineInfo.pushConstantSize = sizeof(uint32_t);
    pipelineMeshOffsetsIndirectCS = LoadPipeline(&pipelineInfo);

    VkDescriptorSetLayout layoutIndices[] =
    {
        descriptorSetLayoutRenderableIndices,
        descriptorSetLayoutRenderables,
        descriptorSetLayoutIndirect
    };
    pipelineInfo.shaderFile = "shaders/calculateIndicesIndirectCS.spv";
    pipelineInfo.descSetLayouts = layoutIndices;
    pipelineInfo.numLayouts = 3;
    pipelineInfo.pushConstantSize = 0;
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
    VkDescriptorBufferInfo bufferDescriptor = {};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        bufferDescriptor.buffer = renderableDataBuffers[i];
        bufferDescriptor.offset = 0;
        bufferDescriptor.range = sizeof(RenderableData) * pScene->numRenderables;

        VkWriteDescriptorSet writeDescSet =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSetsRenderableData[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &bufferDescriptor};
        vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);


        bufferDescriptor.buffer = renderableIndexBuffers[i];
        bufferDescriptor.range = sizeof(uint32_t) * pScene->numRenderables;
        writeDescSet.dstSet = descriptorSetsRenderableIndices[i];
        vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);


        bufferDescriptor.buffer = indirectDataBuffers[i];
        bufferDescriptor.range = sizeof(Mesh) * MAX_MESHES;
        writeDescSet.dstSet = descriptorSetsIndirect[i];
        vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
    }
    renderableCount = pScene->numRenderables;
}

void GfxSetGlobalData(GlobalData &globalData)
{
    memcpy(globalDataBuffers[frameIndex].allocInfo.pMappedData, &globalData, sizeof(globalData));
}

void GfxInit()
{
    GfxCreateWindow();
    GfxCreateInstance();
    GfxSetupDebugMessanger();
    GfxSelectDevice();
    GfxCreateDevice();
    GfxCreateSurface();
    GfxCreateVMA();
    GfxCreateSwapchain();
    GfxCreateSyncObjects();
    GfxCreateCommandBuffer();
    AssetLoaderInit();
    GfxCreateShaderData();
    GfxCreateDescriptorPool();
#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
    GfxCreateDescriptorHeap();
#endif
    GfxCreateBindlessRenderableBuffer();
    GfxCreateRenderableIndexBuffer();
    GfxCreateBindlessDesc();
    GfxCreateIndirectBuffer();
    GfxCreateComputePipelines();
}

int GfxUpdate()
{
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
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 1, 1, &descriptorSetsRenderableData[frameIndex], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 2, 1, &descriptorSetsRenderableIndices[frameIndex], 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->layout, VK_SHADER_STAGE_ALL, 0, sizeof(VkDeviceAddress), &globalDataBuffers[frameIndex].deviceAddress);
#endif

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    VkDeviceSize vOffset{0};

    vkCmdBindVertexBuffers(cmd, 0, 1, GetMeshVertexBuffer(), &vOffset);
    vkCmdBindIndexBuffer(cmd, *GetMeshIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(cmd, indirectDataBuffers[frameIndex], 0, (uint32_t)GetNumMeshes(), sizeof(Mesh));
}

void GfxSetRenderableData(Scene *pScene)
{
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        memcpy(renderableDataAllocInfo[i].pMappedData, pScene->pRenderableData, sizeof(RenderableData) * pScene->numRenderables);
        // memcpy(renderableIndexAllocInfo[i].pMappedData, renderableIndices, sizeof(uint32_t) * pScene->numRenderables);
    }
}

int GfxRender()
{
    VK_CHECK(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &fences[frameIndex]));

    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);

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

    frameIndex = (frameIndex + 1) % FRAMES_IN_FLIGHT;

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex};
    vkQueuePresentKHR(graphicsQueue, &presentInfo);
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

    AssetLoaderDestroy();

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        vmaDestroyBuffer(allocator, globalDataBuffers[i].buffer, globalDataBuffers[i].alloc);

        vkDestroyFence(device, fences[i], nullptr);
        vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);

        vmaDestroyBuffer(allocator, renderableIndexBuffers[i], renderableIndexAllocations[i]);
        vmaDestroyBuffer(allocator, renderableDataBuffers[i], renderableDataAllocations[i]);
        vmaDestroyBuffer(allocator, indirectDataBuffers[i], indirectDataAllocations[i]);
    }
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
    }

    free(renderCompleteSemaphores);

    GfxDestroySwapchain();
    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTex, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutRenderables, nullptr);

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

void GfxCreateRenderableIndexBuffer()
{
    VkBufferCreateInfo bufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(uint32_t) * MAX_RENDERABLES,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(
            vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocCreateInfo,
                &renderableIndexBuffers[i],
                &renderableIndexAllocations[i],
                &renderableIndexAllocInfo[i]));
        VkBufferDeviceAddressInfo bdaInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = renderableIndexBuffers[i]};

        char allocName[96];
        sprintf(allocName, "Renderable Indices Allocation: %i", i);

        vmaSetAllocationName(allocator, renderableIndexAllocations[i], allocName);
    }
}

void GfxCreateBindlessRenderableBuffer()
{
    VkBufferCreateInfo bufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(RenderableData) * MAX_RENDERABLES,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(
            vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocCreateInfo,
                &renderableDataBuffers[i],
                &renderableDataAllocations[i],
                &renderableDataAllocInfo[i]));
        VkBufferDeviceAddressInfo bdaInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = renderableDataBuffers[i]};

        char allocName[96];
        sprintf(allocName, "Renderable Data Allocation: %i", i);

        vmaSetAllocationName(allocator, renderableDataAllocations[i], allocName);
    }
}

void GfxCreateIndirectBuffer()
{
    VkBufferCreateInfo bufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(Mesh) * MAX_MESHES,
            .usage = VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT 
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    VmaAllocationCreateInfo allocCreateInfo =
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(
            vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocCreateInfo,
                &indirectDataBuffers[i],
                &indirectDataAllocations[i],
                &indirectDataAllocInfo[i]));
        VkBufferDeviceAddressInfo bdaInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = indirectDataBuffers[i]};

        char allocName[96];
        sprintf(allocName, "Indirect Data Allocation: %i", i);

        vmaSetAllocationName(allocator, indirectDataAllocations[i], allocName);
    }
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
    VkDescriptorSetLayoutBinding descLayoutBindingRenderable = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = MAX_RENDERABLES,
        .stageFlags = VK_SHADER_STAGE_ALL};
    VkDescriptorSetLayoutCreateInfo descLayoutRenderableCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &descLayoutBindingRenderable};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descLayoutRenderableCreateInfo, NULL, &descriptorSetLayoutRenderables));
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descLayoutRenderableCreateInfo, NULL, &descriptorSetLayoutRenderableIndices));

    variableDescCount = MAX_RENDERABLES;
    variableDescCountAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &variableDescCount};
    VkDescriptorSetAllocateInfo renderableDescSetAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableDescCountAllocInfo,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayoutRenderables};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkAllocateDescriptorSets(device, &renderableDescSetAllocInfo, &descriptorSetsRenderableData[i]));
    }
    renderableDescSetAllocInfo.pSetLayouts = &descriptorSetLayoutRenderableIndices;
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkAllocateDescriptorSets(device, &renderableDescSetAllocInfo, &descriptorSetsRenderableIndices[i]));
    }

    // Mesh Indirect Descriptor Set/Layout
    VkDescriptorSetLayoutBinding descLayoutBindingIndirect = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = MAX_MESHES,
        .stageFlags = VK_SHADER_STAGE_ALL};
    VkDescriptorSetLayoutCreateInfo descLayoutIndirectCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &descLayoutBindingRenderable};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descLayoutIndirectCreateInfo, NULL, &descriptorSetLayoutIndirect));

    variableDescCount = MAX_MESHES;
    variableDescCountAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &variableDescCount};
    VkDescriptorSetAllocateInfo indirectDescSetAllocInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableDescCountAllocInfo,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayoutIndirect};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkAllocateDescriptorSets(device, &indirectDescSetAllocInfo, &descriptorSetsIndirect[i]));
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
VkDescriptorSetLayout GfxGetDescriptorSetLayoutRenderableIndices()
{
    return descriptorSetLayoutRenderableIndices;
}

VkDescriptorSetLayout GfxGetDescriptorSetLayoutRenderables()
{
    return descriptorSetLayoutRenderables;
}
