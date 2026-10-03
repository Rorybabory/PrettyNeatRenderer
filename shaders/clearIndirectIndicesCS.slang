
struct VSInput
{
    float3 Pos;
    float3 Normal;
    float2 UV;
};

struct GlobalData
{
    float4x4 projection;
    float4x4 view;
    float4 lightPos;
    uint32_t selected;
};

// Basically just VkDrawIndexedIndirectCommand
struct Mesh
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t textureID;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Mesh> g_indirectData;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    g_indirectData[threadId.x].firstInstance = 0;
    g_indirectData[threadId.x].instanceCount = 0;
}
