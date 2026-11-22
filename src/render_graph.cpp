#include "render_graph.h"
#include <cstring>
#include <spdlog/spdlog.h>

RGRenderPassID RGAddRenderPass(RGRenderGraph &renderGraph, RGRenderPass &renderPass)
{
    renderGraph.pRenderPasses[renderGraph.numRenderPasses] = renderPass;
    return (RGRenderPassID)renderGraph.numRenderPasses++;
}

RGRenderPassID RGGetRenderPass(RGRenderGraph &renderGraph, const char *name)
{
    for (RGRenderPassID i = 0; i < renderGraph.numRenderPasses; ++i)
    {
        if (strcmp(renderGraph.pRenderPasses[i].name, name) == 0)
        {
            return i;
        }
    }
    return RG_INVALID_RENDER_PASS;
}

void RGCompileRenderGraph(RGRenderGraph &renderGraph, RGCompiledRenderGraph &compiledRenderGraph)
{
    uint32_t remainingPasses = renderGraph.numRenderPasses;

    RGResourceUsage resourceStates[RG_MAX_RENDER_PASSES];
    bool renderPassesRun[RG_MAX_RENDER_PASSES];

    for (uint32_t i = 0; i < RG_MAX_DEPENDENCIES; i++)
    {
        resourceStates[i] = RG_USAGE_UNKNOWN;
        renderPassesRun[i] = false;
    }

    while (remainingPasses)
    {
        for (uint32_t i = 0; i < renderGraph.numRenderPasses; ++i)
        {
            if (renderPassesRun[i] == true)
                continue;

            bool needsBarrier = false;
            bool ranDependencies = true;

            RGRenderPass *pass = &renderGraph.pRenderPasses[i];

            for (uint32_t j = 0; j < pass->numColorAttachments; ++j)
            {
                if (resourceStates[pass->pColorAttachments[j]] != RG_USAGE_COLOR_ATTACHMENT)
                {
                    needsBarrier = true;
                    break;
                }
            }
            if (!needsBarrier)
            {
                for (uint32_t j = 0; j < pass->numShaderReads; ++j)
                {
                    if (resourceStates[pass->pShaderReads[j]] != RG_USAGE_SHADER_READ)
                    {
                        needsBarrier = true;
                        break;
                    }
                }
            }
            if (!needsBarrier && pass->depthStencilAttachment != UINT16_MAX)
            {
                if (resourceStates[pass->depthStencilAttachment] != RG_USAGE_DEPTH_STENCIL_ATTACHMENT)
                {
                    needsBarrier = true;
                    break;
                }
            }
            for (uint32_t j = 0; j < pass->numDependencies; ++j)
            {
                if (!renderPassesRun[pass->pDependencies[j]])
                {
                    ranDependencies = false;
                }
            }
            if (ranDependencies)
            {
                // Render pass is good to go, add it to the compiled render graph
                renderPassesRun[i] = true;

                RGCompiledStep step = {};

                step.renderPass = *pass;
                step.numBarriers = 0;
                compiledRenderGraph.steps[compiledRenderGraph.numSteps] = step;
                compiledRenderGraph.numSteps++;
                remainingPasses--;
                break;
            }
        }
    }
}

void RGDebugCompiledRenderGraph(RGCompiledRenderGraph &compiledRenderGraph)
{
    spdlog::info("------------------------------ Begin Debugging Render Graph ------------------------------");
    for (uint32_t i = 0; i < compiledRenderGraph.numSteps; ++i)
    {
        spdlog::info("Render Pass: {}", compiledRenderGraph.steps[i].renderPass.name);
    }
    spdlog::info("------------------------------ End Debugging Render Graph --------------------------------");
}

void RGTestRenderGraph()
{
    RGRenderGraph renderGraph;
    RGCompiledRenderGraph compiledRenderGraph;

    RGRenderPass testPass1 = {};
    testPass1.name = "Test Pass 1";
    testPass1.numDependencies = 0;

    RGRenderPassID id1 = RGAddRenderPass(renderGraph, testPass1);

    RGRenderPass testPass2 = {};
    testPass2.name = "Test Pass 2";
    testPass2.numDependencies = 0;
    RGRenderPassID id2 = RGAddRenderPass(renderGraph, testPass2);

    RGRenderPass testPass3 = {};
    testPass3.name = "Test Pass 3";
    testPass3.numDependencies = 2;
    testPass3.pDependencies[0] = id1;
    testPass3.pDependencies[1] = id2;
    RGAddRenderPass(renderGraph, testPass3);

    RGCompileRenderGraph(renderGraph, compiledRenderGraph);

    RGDebugCompiledRenderGraph(compiledRenderGraph);
}