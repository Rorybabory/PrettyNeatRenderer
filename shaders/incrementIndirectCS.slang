import Common;


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
