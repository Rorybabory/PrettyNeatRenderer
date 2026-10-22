#pragma once

#include "glm/ext/vector_float3.hpp"
#include "volk.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include "resource.h"
#define FRAMES_IN_FLIGHT 2
// #define DESCRIPTOR_HEAP_EXPERIMENTS

struct GlobalData
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
};

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

void GfxUpdateTextures(Texture *textures, uint32_t count, uint32_t offset);

void GfxCreateScene(Scene *pScene);
void GfxUpdateScene(Scene *pScene);

void GfxSetGlobalData(GlobalData &globalData);

void GfxSetRenderableData(Scene *pScene);
void GfxSortRenderables(Scene &scene);

void GfxSetMesh(Mesh *mesh);

void GfxSetPipeline(Pipeline *_pipeline);

VkCommandBuffer GfxCreateOneTimeCmdBuffer();

float GfxGetAspectRatio();

void GfxGetBindingMappings(VkDescriptorSetAndBindingMappingEXT **bindingMappings, uint32_t *bindingCount);
VkFormat GfxGetDepthFormat();
VkDescriptorSetLayout GfxGetDescriptorSetLayoutTextures();
VkDescriptorSetLayout GfxGetDescriptorSetLayoutRenderables();
VkDescriptorSetLayout GfxGetDescriptorSetLayoutRenderableIndices();

VkCommandPool GfxGetCommandPool();

VkQueue GfxGetQueue();