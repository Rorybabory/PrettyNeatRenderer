#pragma once

#include "resource.h"


void AssetLoaderInit();
void AssetLoaderDestroy();

struct MeshLoadInfo
{
    const char * filename;
    uint32_t flags;
};
MeshID LoadMesh(MeshLoadInfo * loadInfo);
Mesh * GetMesh(MeshID id);
uint32_t GetNumMeshes();


struct TextureLoadInfo
{
    const char * filename;
    uint32_t flags;
};
bool LoadTexture(TextureLoadInfo * loadInfo, Texture & texture);
void DestroyTexture(Texture & texture);

struct RenderTargetCreateInfo
{
    const char * name;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    bool isDepth;
};
bool CreateRenderTarget(RenderTargetCreateInfo * createInfo, Texture & texture);

struct PipelineLoadInfo
{
    const char * shaderFile;
    VkDescriptorSetLayout * descSetLayouts;
    uint32_t numLayouts;
    uint32_t flags;
};

PipelineID LoadPipeline(PipelineLoadInfo * pipelineInfo);
Pipeline * GetPipeline(PipelineID id);
void ClearPipelineRegistry();


VkBuffer * GetMeshVertexBuffer();
VkBuffer * GetMeshIndexBuffer();
