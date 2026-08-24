#pragma once

#include "resource.h"


void AssetLoaderInit();
void AssetLoaderDestroy();

struct MeshLoadInfo
{
    const char * filename;
    uint32_t flags;
};
bool LoadMesh(MeshLoadInfo * loadInfo, Mesh & mesh);


struct TextureLoadInfo
{
    const char * filename;
    uint32_t flags;
};
bool LoadTexture(TextureLoadInfo * loadInfo, Texture & texture);
void DestroyTexture(Texture & texture);

struct PipelineLoadInfo
{
    const char * shaderFile;
    VkDescriptorSetLayout descSetLayout;
    uint32_t flags;
};

bool LoadPipeline(PipelineLoadInfo * pipelineInfo, Pipeline & pipeline);
void DestroyPipeline(Pipeline & pipeline);

VkBuffer * GetMeshBuffer();
VkBuffer * GetMeshIndexBuffer();