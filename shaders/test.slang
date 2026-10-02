
struct VSInput {
    float3 Pos;
    float3 Normal;
    float2 UV;
};


struct GlobalData {
    float4x4 projection;
    float4x4 view;
    float4 lightPos;
    uint32_t selected;
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

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t textureID;
};

[[vk::binding(0, 0)]] Sampler2D g_textures[512];
[[vk::binding(0, 1)]] StructuredBuffer<RenderableData> renderableData;

[shader("vertex")]
VSOutput main(VSInput input, uniform GlobalData *globalData, uint instanceIndex : SV_VulkanInstanceID) 
{
    VSOutput output;
    float4x4 modelMat4 = float4x4(
        renderableData[instanceIndex].transformRow0,
        renderableData[instanceIndex].transformRow1,
        renderableData[instanceIndex].transformRow2,
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    );
    output.Normal = mul((float3x3)mul(globalData->view, modelMat4), input.Normal);
    output.UV = input.UV;
    output.Pos = mul(globalData->projection, mul(globalData->view, mul(modelMat4, float4(input.Pos.xyz, 1.0))));
    output.Factor = 1.0f;
    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(globalData->view, modelMat4), float4(input.Pos.xyz, 1.0));
    output.LightVec = globalData->lightPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;
    output.textureID = renderableData[instanceIndex].textureIDs_0_3.x;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) 
{
    // Phong lighting
    float3 N = normalize(input.Normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);
    float3 R = reflect(-L, N);
    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
    // Sample from texture
    float3 color = g_textures[input.textureID].Sample(input.UV).rgb * input.Factor;
    return float4(diffuse * color.rgb + specular, 1.0);
}
