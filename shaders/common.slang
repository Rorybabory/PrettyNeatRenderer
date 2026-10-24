
module Common;

// Basically just VkDrawIndexedIndirectCommand
public struct Mesh
{
    public uint32_t indexCount;
    public Atomic<uint32_t> instanceCount;
    public uint32_t firstIndex;
    public int32_t vertexOffset;
    public uint32_t firstInstance;

    public Atomic<uint32_t> meshOffset;
    public uint32_t padding0;
    public uint32_t padding1;
};

public struct RenderableData
{
    public uint16_t4 textureIDs_0_3;
    public uint16_t2 textureIDs_4_5;
    public uint16_t mesh;
    public uint16_t _padding;

    public float4 transformRow0;
    public float4 transformRow1;
    public float4 transformRow2;
};

public struct GlobalData
{
    public float4x4 projection;
    public float4x4 view;
    public float4 lightPos;
    public uint32_t selected;
};
