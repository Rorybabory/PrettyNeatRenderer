#pragma once

#include <glm/glm.hpp>
#include "volk.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#define MAX_RENDERABLES 262144
#define MAX_PIPELINES 65535
#define MAX_MESHES 65535
#define MAX_DESCRIPTOR_SETS 4

struct Vertex 
{
    glm::vec3 mPos;
    glm::vec3 mNormal;
    glm::vec2 mUv;
};

typedef uint16_t MeshID;
typedef VkDrawIndexedIndirectCommand Mesh;

struct Texture 
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkSampler sampler;
    VkImageLayout imageLayout;
    VkFormat format;
    uint32_t layerCount;
};

typedef uint16_t PipelineID;
struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct RenderableCreateInfo
{
    uint16_t textureIDs[8];
    glm::mat4 transform;
    glm::vec4 tint;
    PipelineID pipelineID;
    MeshID meshID;
};

typedef glm::mat4x3 float4x3;

struct RenderableData
{
    uint16_t textureIDs[6];
    uint16_t mesh;
    uint16_t _padding;

    glm::vec4 transformRow0;
    glm::vec4 transformRow1;
    glm::vec4 transformRow2;
};

struct Scene
{
    RenderableData * pRenderableData;
    uint32_t numRenderables = 0;
};
