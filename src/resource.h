#pragma once

#include <glm/glm.hpp>
#include "volk.h"
#include <vk_mem_alloc.h>
struct Vertex 
{
    glm::vec3 mPos;
    glm::vec3 mNormal;
    glm::vec2 mUv;
};

struct Mesh 
{
    uint32_t vertexCount;
    uint32_t indexCount;
    VkDeviceSize vertexBufferSize;
    VkDeviceSize offset;
};

struct Texture 
{
    VkImage image;
    VmaAllocation allocation;
    VkSampler sampler;
    VkImageLayout imageLayout;
    VkFormat format;
    uint32_t layerCount;
};

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct Renderable 
{
    uint16_t textureIDs[8];
    glm::vec4 color;
    uint32_t transformID;
    uint16_t pipelineID;
    uint16_t meshID;
};
struct Scene
{
    Renderable * pRenderables;
};
struct PushData
{
    uint16_t textureIDs[8];
    glm::vec4 color;
    uint32_t transformID;
};