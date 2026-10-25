import Common;


[[vk::binding(0, 0)]]
RWStructuredBuffer<Mesh> g_indirectData;
[[vk::binding(0, 1)]]
RWStructuredBuffer<RenderableData> renderableData;

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
        uint16_t numLODs = g_indirectData[meshID].numLods;
        if (numLODs > 0)
        {
            float4 renderablePos = float4(renderableData[threadId.x].transformRow0.w,
                                          renderableData[threadId.x].transformRow1.w,
                                          renderableData[threadId.x].transformRow2.w,
                                          1.0f);

            float distance = length(renderablePos - pGlobalData->cameraPos) * 1.25f;

            uint index = clamp(uint(floor(distance+1)), 0, numLODs);
            if (distance > 1.0)
            {
                meshID = g_indirectData[meshID].lodMeshes[index-1];
            }
        }
        renderableData[threadId.x].meshOverride = meshID;
        g_indirectData[renderableData[threadId.x].meshOverride].instanceCount++;
    } else {
        renderableData[threadId.x].meshOverride = 0xFFFF;
    }
}
