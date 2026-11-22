import Common;

struct MeshOffsetsPushConstants
{
    Mesh *g_indirectData;
    uint32_t numMeshes;
};

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uniform MeshOffsetsPushConstants push_constants, uint3 threadId: SV_DispatchThreadID)
{
    if (threadId.x == 0) {
        uint32_t currentOffset = 0;
        for (uint i = 0; i < push_constants.numMeshes; ++i)
        {
            push_constants.g_indirectData[i].firstInstance = currentOffset;
            push_constants.g_indirectData[i].meshOffset = currentOffset;
            currentOffset += push_constants.g_indirectData[i].instanceCount.load();
        }
    }
}
  