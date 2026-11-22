import Common;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint32_t> renderableIndices;
[[vk::binding(0, 1)]]
RWStructuredBuffer<RenderableData> renderableData;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uniform Mesh * g_indirectData, uint3 threadId: SV_DispatchThreadID)
{
    uint numRenderables;
    uint stride;
    renderableData.GetDimensions(numRenderables, stride);
    if (threadId.x >= numRenderables)
        return;

    uint16_t meshID = renderableData[threadId.x].meshOverride;
    if (meshID == 0xFFFF)
    {
        return;
    }

    uint32_t writeIndex = g_indirectData[meshID].meshOffset++;
    renderableIndices[writeIndex] = threadId.x;
}
