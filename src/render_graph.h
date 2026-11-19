#pragma once
#include "resource.h"

#define RG_MAX_RENDER_PASSES 128
#define RG_MAX_DEPENDENCIES 18
#define RG_INVALID_RENDER_PASS -1
#define MAX_COMPILED_BARRIERS 64

typedef uint32_t RGRenderPassID;


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
    RG_USAGE_COLOR_ATTACHMENT,
};
enum RGResourceType 
{
    RG_RESOURCE_BUFFER,
    RG_RESOURCE_IMAGE
};
struct RGRenderPass;

typedef struct RGRenderPass
{
    void(*function)( VkCommandBuffer cmd, RGRenderPass * pRenderPass );
    
    TextureID * pColorAttachments;
    uint32_t numColorAttachments = 0;

    TextureID * pShaderReads;
    uint32_t numShaderReads = 0;

    TextureID depthStencilAttachment = UINT16_MAX;

    RGRenderPassID dependencies[RG_MAX_DEPENDENCIES];
    uint32_t numDependencies;

    const char * name;

} RGRenderPass;

typedef struct RGRenderGraph
{
    RGRenderPass pRenderPasses[RG_MAX_RENDER_PASSES];
    uint32_t numRenderPasses = 0;
} RGRenderGraph;


typedef struct RGCompiledBarrier
{
    RGResourceType resourceType;
    RGResourceStage stage;
    RGResourceUsage previousUsage;
    RGResourceUsage newUsage;
} RGCompiledBarrier;

typedef struct RGCompiledStep
{
    RGRenderPass renderPass;
    RGCompiledBarrier pBarriers[MAX_COMPILED_BARRIERS];
    uint32_t numBarriers;
} RGCompiledStep;

typedef struct RGCompiledRenderGraph
{
    RGCompiledStep steps[RG_MAX_RENDER_PASSES];
    uint32_t numSteps = 0;
};

RGRenderPassID RGGetRenderPass(RGRenderGraph & renderGraph, const char * name);
RGRenderPassID RGAddRenderPass(RGRenderGraph & renderGraph, RGRenderPass & renderPass);
void RGCompileRenderGraph(RGRenderGraph & renderGraph, RGCompiledRenderGraph & compiledRenderGraph);
void RGDebugCompiledRenderGraph(RGCompiledRenderGraph & compiledRenderGraph);

void RGTestRenderGraph();