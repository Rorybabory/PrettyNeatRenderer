#pragma once

#include "glm/ext/vector_float3.hpp"
#include "volk.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include "resource.h"
// #define DESCRIPTOR_HEAP_EXPERIMENTS

#define USE_OPENXR


struct GfxInstance;

void GfxInit();

void GfxSceneAddRenderable(Scene &scene, RenderableCreateInfo *createInfo);
glm::mat4 GfxGenerateModelMatrix(glm::vec3 position, glm::vec3 scale, glm::vec3 rotationEular);

int GfxUpdate();

int GfxRender();

void GfxPreDestroy();
void GfxDestroy();

GLFWwindow * GfxGetWindow();

void GfxWaitForFrame();

#ifdef DESCRIPTOR_HEAP_EXPERIMENTS
void GfxCreateDescriptorHeap();
#endif
void GfxCreateBindlessDesc();
void GfxCreateBindlessRenderableBuffer();
void GfxCreateRenderableIndexBuffer();
void GfxCreateIndirectBuffer();
void GfxCreateComputePipelines();
void GfxCreateImGui();
void GfxImguiNewFrame();
#ifdef USE_OPENXR
void GfxCreateOpenXRInstance();
void GfxCreateOpenXRSession();
void GfxHandleOpenXREvents();
void GfxGetViewConfigurationViews();
void GfxCreateXRSwapchain();
#endif
void GfxUpdateTextures(Texture *textures, uint32_t count, uint32_t offset);

void GfxCreateScene(Scene *pScene);
void GfxUpdateScene(Scene *pScene);

void GfxSetCameraData(glm::mat4 & cameraView, glm::vec3 cameraPos, float fov, float nearPlane, float farPlane, bool updateFrustum);

void GfxSetRenderableData(Scene *pScene);
void GfxSortRenderables(Scene &scene);

void GfxSetPipeline(Pipeline *_pipeline);

VkCommandBuffer GfxCreateOneTimeCmdBuffer();

float GfxGetAspectRatio();

void GfxGetBindingMappings(VkDescriptorSetAndBindingMappingEXT **bindingMappings, uint32_t *bindingCount);
VkFormat GfxGetDepthFormat();
VkDescriptorSetLayout GfxGetDescriptorSetLayoutTextures();

VkCommandPool GfxGetCommandPool();

VkQueue GfxGetQueue();