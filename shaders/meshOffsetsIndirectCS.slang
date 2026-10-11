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
void main(uniform uint32_t numMeshes, uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x == 0) {
        uint32_t currentOffset = 0;
        for (uint i = 0; i < numMeshes; ++i)
        {
            g_indirectData[i].firstInstance = currentOffset;
            g_indirectData[i].meshOffset = currentOffset;
            currentOffset += g_indirectData[i].instanceCount;
        }
    }
}
