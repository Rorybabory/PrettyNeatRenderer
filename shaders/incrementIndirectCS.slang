import Common;


struct IncrementIndirectPushConstants
{
    GlobalData *g_GlobalData;
    Mesh *g_indirectData;
    RenderableData *renderableData;
    uint32_t numRenderables;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uniform IncrementIndirectPushConstants pushConstants, uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x >= pushConstants.numRenderables)
        return;
    uint16_t meshID = pushConstants.renderableData[threadId.x].mesh;
    if (isInsideFrustum(
            pushConstants.g_GlobalData,
            pushConstants.renderableData[threadId.x].transformRow0,
            pushConstants.renderableData[threadId.x].transformRow1,
            pushConstants.renderableData[threadId.x].transformRow2,
            pushConstants.g_indirectData[meshID].bbMin, pushConstants.g_indirectData[meshID].bbMax))
    {
        uint16_t numLODs = pushConstants.g_indirectData[meshID].numLods;
        if (numLODs > 0)
        {
            float4 renderablePos = float4(pushConstants.renderableData[threadId.x].transformRow0.w,
                                          pushConstants.renderableData[threadId.x].transformRow1.w,
                                          pushConstants.renderableData[threadId.x].transformRow2.w,
                                          1.0f);

            float distance = length(renderablePos - pushConstants.g_GlobalData->cameraPos) * 0.1f;

            uint index = clamp(uint(floor(distance+1)), 0, numLODs);
            if (distance > 1.0)
            {
                meshID = pushConstants.g_indirectData[meshID].lodMeshes[index-1];
            }
        }
        pushConstants.renderableData[threadId.x].meshOverride = meshID;
        pushConstants.g_indirectData[pushConstants.renderableData[threadId.x].meshOverride].instanceCount++;
    } else {
        pushConstants.renderableData[threadId.x].meshOverride = 0xFFFF;
    }
}
