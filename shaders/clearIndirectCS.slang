// Basically just VkDrawIndexedIndirectCommand
struct Mesh
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;

    uint32_t meshOffset;
    uint32_t padding0;
    uint32_t padding1;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Mesh> g_indirectData;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    g_indirectData[threadId.x].firstInstance = 0;
    g_indirectData[threadId.x].instanceCount = 0;
    g_indirectData[threadId.x].meshOffset = 0;
}
