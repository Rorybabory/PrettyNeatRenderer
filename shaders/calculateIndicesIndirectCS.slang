// Basically just VkDrawIndexedIndirectCommand
struct Mesh
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;

    Atomic<uint32_t> meshOffset;
    uint32_t padding0;
    uint32_t padding1;
};
struct RenderableData
{
    uint16_t4 textureIDs_0_3;
    uint16_t2 textureIDs_4_5;
    uint16_t mesh;
    uint16_t _padding;

    float4 transformRow0;
    float4 transformRow1;
    float4 transformRow2;
};

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
