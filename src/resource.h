#pragma once

#include <glm/glm.hpp>
#include "volk.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#define MAX_RENDERABLES 1048576
#define MAX_PIPELINES 65535
#define MAX_MESHES 65535
#define MAX_TEXTURES 512
#define INVALID_TEXTURE UINT16_MAX
#define MAX_DESCRIPTOR_SETS 4

struct Vertex
{
    glm::vec3 mPos;
    glm::vec3 mNormal;
    glm::vec2 mUv;
};

typedef uint16_t MeshID;
typedef uint16_t TextureID;
typedef uint16_t BufferID; //Currently Unused

typedef struct Mesh
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
    uint32_t meshOffset;
    glm::vec3 bbMin;
    glm::vec3 bbMax;
    uint16_t lodMeshes[5];
    uint16_t numLods;
    uint16_t padding;
} Mesh;

enum TextureFlags
{
    TEXTURE_FLAG_NONE = 0x00000000,
    TEXTURE_FLAG_COLOR_ATTACHMENT = 0x00000001,
    TEXTURE_FLAG_DEPTH_STENCIL_ATTACHMENT = 0x00000002
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
    TextureFlags flags;
    const char *debug_name;
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
    uint16_t meshOverride;

    glm::vec4 transformRow0;
    glm::vec4 transformRow1;
    glm::vec4 transformRow2;
};

struct Scene
{
    RenderableData *pRenderableData;
    uint32_t numRenderables = 0;
};

struct GlobalData
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    glm::vec4 frustumPlanes[6];
    glm::vec4 cameraPos{0.0f, 0.0f, 0.0f, 1.0f};
};