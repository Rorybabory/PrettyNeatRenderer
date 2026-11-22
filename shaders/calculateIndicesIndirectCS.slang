import Common;

struct CalculateIndicesPushConstants
{
    Mesh *g_indirectData;
    RenderableData *renderableData;
    uint32_t *renderableIndices;
    uint32_t numRenderables;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uniform CalculateIndicesPushConstants pushConstants, uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x >= pushConstants.numRenderables)
        return;

    uint16_t meshID = pushConstants.renderableData[threadId.x].meshOverride;
    if (meshID == 0xFFFF)
    {
        return;
    }

    uint32_t writeIndex = pushConstants.g_indirectData[meshID].meshOffset++;
    pushConstants.renderableIndices[writeIndex] = threadId.x;
}
