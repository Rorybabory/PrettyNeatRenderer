#pragma once

#include "resource.h"

void AssetLoaderInit();
void AssetLoaderDestroy();

struct MeshLoadInfo
{
    const char *filename;
    uint32_t flags;
    MeshID *pLods = NULL;
    uint16_t numLods = 0;
};
MeshID LoadMesh(MeshLoadInfo *loadInfo);
Mesh *GetMesh(MeshID id);
uint32_t GetNumMeshes();

struct TextureLoadInfo
{
    const char *filename;
    uint32_t flags;
};
TextureID LoadTexture(TextureLoadInfo *loadInfo);
void ClearTextureRegistry();

struct RenderTargetCreateInfo
{
    const char *name;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    bool isDepth;
};
TextureID CreateRenderTarget(RenderTargetCreateInfo *createInfo);



struct BufferCreateInfo
{
    const char * debugName;
    uint64_t size;
    uint64_t flags;
};
BufferID CreateBuffer(BufferCreateInfo * pCreateInfo);
VkDeviceAddress GetBufferDeviceAddress(BufferID bufferID, uint32_t frameIndex = 0);
void * GetBufferMappedAddress(BufferID bufferID, uint32_t frameIndex = 0);
VkBuffer GetBufferVK(BufferID bufferID, uint32_t frameIndex = 0);
void ClearBufferRegistry();

enum PipelineFlags
{
    PIPELINE_FLAG_NONE = 0,
    PIPELINE_FLAG_GRAPHICS = 1,
    PIPELINE_FLAG_COMPUTE = 2,
};

struct PipelineLoadInfo
{
    const char *shaderFile;
    VkDescriptorSetLayout *descSetLayouts;
    uint32_t numLayouts;
    PipelineFlags flags;
    uint32_t pushConstantSize;
};

PipelineID LoadPipeline(PipelineLoadInfo *pipelineInfo);
Pipeline *GetPipeline(PipelineID id);
void ClearPipelineRegistry();

VkBuffer GetMeshVertexBuffer();
VkBuffer GetMeshIndexBuffer();

const char *GetTextureDebugName(TextureID id);
void SetTextureDebugName(TextureID id, const char *name);