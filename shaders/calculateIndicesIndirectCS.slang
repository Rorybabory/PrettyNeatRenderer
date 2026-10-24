import Common;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint32_t> renderableIndices;
[[vk::binding(0, 1)]]
StructuredBuffer<RenderableData> renderableData;
[[vk::binding(0, 2)]]
RWStructuredBuffer<Mesh> g_indirectData;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    uint16_t meshID = renderableData[threadId.x].mesh;
    uint32_t writeIndex = g_indirectData[meshID].meshOffset++;
    renderableIndices[writeIndex] = threadId.x;
}
