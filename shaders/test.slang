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

#define MAX_RENDERABLES 4095

struct RenderableData
{
    uint16_t textureIDs[8];
    float4x4 transform;
    float4 tint;
    float4 unused0;
    float4 unused1;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
};

[[vk::binding(0, 0)]] uniform Sampler2D g_textures[];

[[vk::binding(0, 1)]] StructuredBuffer<RenderableData> renderableData;

[shader("vertex")]
VSOutput main(VSInput input, uniform GlobalData *globalData, uint instanceIndex : SV_VulkanInstanceID) 
{
    VSOutput output;
    float4x4 modelMat = renderableData[instanceIndex].transform;
    output.Normal = mul((float3x3)mul(globalData->view, modelMat), input.Normal);
    output.UV = input.UV;
    output.Pos = mul(globalData->projection, mul(globalData->view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
    output.Factor = 1.0f;
    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(globalData->view, modelMat), float4(input.Pos.xyz, 1.0));
    output.LightVec = globalData->lightPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;
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
    float3 color = g_textures[0].Sample(input.UV).rgb * input.Factor;
    return float4(diffuse * color.rgb + specular, 1.0);
}
