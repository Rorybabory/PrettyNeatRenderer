import Common;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint32_t> renderableIndices;
[[vk::binding(0, 1)]]
StructuredBuffer<RenderableData> renderableData;
[[vk::binding(0, 2)]]
RWStructuredBuffer<Mesh> g_indirectData;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uniform GlobalData * pGlobalData, uint3 threadId: SV_DispatchThreadID)
{
    uint numRenderables;
    uint stride;
    renderableData.GetDimensions(numRenderables, stride);
    if (threadId.x >= numRenderables)
        return;
    
    uint16_t meshID = renderableData[threadId.x].mesh;
    if (isInsideFrustum(
        pGlobalData,
        renderableData[threadId.x].transformRow0,
        renderableData[threadId.x].transformRow1,
        renderableData[threadId.x].transformRow2,
        g_indirectData[meshID].bbMin, g_indirectData[meshID].bbMax)) 
        {
            uint32_t writeIndex = g_indirectData[meshID].meshOffset++;
            renderableIndices[writeIndex] = threadId.x;
        }
    
}
