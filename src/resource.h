#pragma once

#include <glm/glm.hpp>
#include "volk.h"
#include <vk_mem_alloc.h>

#define MAX_RENDERABLES 4095
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

//128 bytes large, keeping it this size makes it significantly easier to manage
struct RenderableData
{
    uint16_t textureIDs[8];
    glm::mat4 transform;
    glm::vec4 tint;
    glm::vec4 unused0;
    glm::vec4 unused1;
};

struct Renderable 
{
    uint32_t index;
    PipelineID pipelineID;
    MeshID meshID;
};


struct Scene
{
    Renderable pRenderables[MAX_RENDERABLES];
    RenderableData pRenderableData[MAX_RENDERABLES];
    uint32_t numRenderables = 0;
};
