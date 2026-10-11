// Basically just VkDrawIndexedIndirectCommand
struct Mesh
{
    uint32_t indexCount;
    Atomic<uint32_t> instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;

    uint32_t meshOffset;
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
RWStructuredBuffer<Mesh> g_indirectData;
[[vk::binding(0, 1)]]
StructuredBuffer<RenderableData> renderableData;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    g_indirectData[renderableData[threadId.x].mesh].instanceCount++;
}
