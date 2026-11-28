#pragma once
#include "resource.h"

#define RG_MAX_RENDER_PASSES 128
#define RG_MAX_DEPENDENCIES 16
#define RG_INVALID_RENDER_PASS -1
#define RG_MAX_COMPILED_BARRIERS 64
#define RG_MAX_SHADER_READS 16
#define RG_MAX_COLOR_ATTACHMENTS 8
#define RG_MAX_RW_BUFFERS 8
#define RG_MAX_READ_BUFFERS 16

typedef uint32_t RGRenderPassID;
typedef uint64_t RGRenderPassFlags;

enum RGResourceStage
{
    RG_STAGE_VERTEX_SHADER,
    RG_STAGE_VERTEX_INPUT,
    RG_STAGE_FRAGMENT_SHADER,
    RG_STAGE_COLOR_ATTACHMENT_OUTPUT
};
enum RGResourceUsage
{
    RG_USAGE_UNKNOWN = -1,
    RG_USAGE_SHADER_READ,
    RG_USAGE_SHADER_WRITE,
    RG_USAGE_DEPTH_STENCIL_ATTACHMENT,
    RG_USAGE_COLOR_ATTACHMENT
};
enum RGResourceType
{
    RG_RESOURCE_BUFFER,
    RG_RESOURCE_IMAGE
};
struct RGRenderPass;

struct RGRenderPass
{
    const char *name;
    void (*function)(VkCommandBuffer cmd, RGRenderPass *pRenderPass);
    RGRenderPassFlags flags;

    TextureID pColorAttachments[RG_MAX_COLOR_ATTACHMENTS];
    TextureID pShaderReads[RG_MAX_SHADER_READS];
    BufferID pRWBuffers[RG_MAX_RW_BUFFERS];
    BufferID pReadBuffers[RG_MAX_READ_BUFFERS];
    RGRenderPassID pDependencies[RG_MAX_DEPENDENCIES];

    TextureID depthStencilAttachment = INVALID_TEXTURE;
    uint16_t numColorAttachments = 0;
    uint16_t numShaderReads = 0;
    uint16_t numRWBuffers = 0;
    uint16_t numReadBuffers = 0;
    uint16_t numDependencies = 0;

    uint8_t padding[54]; // Padded to 256 bytes for convenience, as more things will be added later
};

struct RGRenderGraph
{
    RGRenderPass pRenderPasses[RG_MAX_RENDER_PASSES];
    uint32_t numRenderPasses = 0;
};

struct RGCompiledBarrier
{
    RGResourceType resourceType;
    RGResourceStage previousStage;
    RGResourceStage newStage;
    RGResourceUsage previousUsage;
    RGResourceUsage newUsage;
};

struct RGCompiledStep
{
    RGRenderPass renderPass;
    RGCompiledBarrier pBarriers[RG_MAX_COMPILED_BARRIERS];
    uint32_t numBarriers;
};

struct RGCompiledRenderGraph
{
    RGCompiledStep steps[RG_MAX_RENDER_PASSES];
    uint32_t numSteps = 0;
};

RGRenderPassID RGGetRenderPass(RGRenderGraph &renderGraph, const char *name);
RGRenderPassID RGAddRenderPass(RGRenderGraph &renderGraph, RGRenderPass &renderPass);
void RGCompileRenderGraph(RGRenderGraph &renderGraph, RGCompiledRenderGraph &compiledRenderGraph);
void RGDebugCompiledRenderGraph(RGCompiledRenderGraph &compiledRenderGraph);

void RGExecuteCompiledRenderGraph(RGCompiledRenderGraph * compiledRenderGraph, VkCommandBuffer cmd);

void RGTestRenderGraph();