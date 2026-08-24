#pragma once

#include "volk.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "resource.h"
#define FRAMES_IN_FLIGHT 2

struct ShaderData
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected = 1;
};

struct GfxInstance;

void GfxInit();

int GfxUpdate();

int GfxRender();

void GfxPreDestroy();
void GfxDestroy();

void GfxCreateDescriptorHeap();
void GfxCreateBindlessTextureDesc();
void GfxUpdateTextures(Texture * textures, uint32_t count, uint32_t offset);

void GfxSetShaderData(ShaderData & shaderData);

void GfxCreateRenderable(Renderable * pRenderable);

void GfxSetMesh(Mesh * mesh);

void GfxSetPipeline(Pipeline * _pipeline);

float GfxGetAspectRatio();


void GfxGetBindingMappings(VkDescriptorSetAndBindingMappingEXT ** bindingMappings, uint32_t * bindingCount);
VkFormat GfxGetDepthFormat();
VkDescriptorSetLayout GfxGetDescriptorSetLayout();