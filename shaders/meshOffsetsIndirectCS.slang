import Common;

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
            currentOffset += g_indirectData[i].instanceCount.load();
        }
    }
}
