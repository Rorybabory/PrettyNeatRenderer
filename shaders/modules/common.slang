
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
    public float3 bbMin;
    public float3 bbMax;
    public uint16_t lodMeshes[5];
    public uint16_t numLods;
    public uint16_t padding;
};

public struct RenderableData
{
    public uint16_t4 textureIDs_0_3;
    public uint16_t2 textureIDs_4_5;
    public uint16_t mesh;
    public uint16_t meshOverride;

    public float4 transformRow0;
    public float4 transformRow1;
    public float4 transformRow2;
};

public struct GlobalData
{
    public float4x4 projection;
    public float4x4 view;
    public float4 lightPos;
    public float4 frustumPlanes[6];
    public float4 cameraPos;
};

public bool isInsideFrustum(GlobalData * pGlobalData, float4 transformRow0, float4 transformRow1, float4 transformRow2, float3 bbMin, float3 bbMax)
{
    float4x4 modelMat = float4x4(
        transformRow0,
        transformRow1,
        transformRow2,
        float4(0.0f, 0.0f, 0.0f, 1.0f));
    float3 newMin = float3(transformRow0.w, transformRow1.w, transformRow2.w);
    float3 newMax = float3(transformRow0.w, transformRow1.w, transformRow2.w);
    for (uint i = 0; i < 3; ++i)
    {
        for (uint j = 0; j < 3; ++j)
        {
            float a = modelMat[i][j] * bbMin[j];
            float b = modelMat[i][j] * bbMax[j];

            newMin[i] += min(a, b);
            newMax[i] += max(a, b);
        }
    }
    for (uint i = 0; i < 6; ++i) {
        const float4 g = pGlobalData->frustumPlanes[i];
        if ((dot(g, float4(newMin.x, newMin.y, newMin.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMax.x, newMin.y, newMin.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMin.x, newMax.y, newMin.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMax.x, newMax.y, newMin.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMin.x, newMin.y, newMax.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMax.x, newMin.y, newMax.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMin.x, newMax.y, newMax.z, 1.0f)) < 0.0) &&
            (dot(g, float4(newMax.x, newMax.y, newMax.z, 1.0f)) < 0.0))
        {
            return false;
        }
    }

    return true;
}