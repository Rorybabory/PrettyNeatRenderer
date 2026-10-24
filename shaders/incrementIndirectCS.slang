import Common;


[[vk::binding(0, 0)]]
RWStructuredBuffer<Mesh> g_indirectData;
[[vk::binding(0, 1)]]
StructuredBuffer<RenderableData> renderableData;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uniform GlobalData * pGlobalData, uint3 threadId: SV_DispatchThreadID)
{
    uint numRenderables;
    uint stride;
    renderableData.GetDimensions(numRenderables, stride);
    if (threadId.x >= numRenderables)
        return;
    
    if (isInsideFrustum(
            pGlobalData,
            renderableData[threadId.x].transformRow0,
            renderableData[threadId.x].transformRow1,
            renderableData[threadId.x].transformRow2,
            g_indirectData[renderableData[threadId.x].mesh].bbMin, g_indirectData[renderableData[threadId.x].mesh].bbMax))
    {
        g_indirectData[renderableData[threadId.x].mesh].instanceCount++;
    }
        
}
