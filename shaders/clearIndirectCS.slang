import Common;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(Mesh * g_indirectData, uint3 threadId: SV_DispatchThreadID)
{
    g_indirectData[threadId.x].firstInstance = 0;
    g_indirectData[threadId.x].instanceCount.store(0);
    g_indirectData[threadId.x].meshOffset.store(0); 
}
