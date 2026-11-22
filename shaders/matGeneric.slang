import Common;

struct VSInput
{
    float3 Pos;
    float3 Normal;
    float2 UV;
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
Sampler2D g_textures[512];

struct MatGenericPushConstants
{
    GlobalData *globalData;
    RenderableData *renderableData;
    uint32_t *renderableIndices;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform MatGenericPushConstants pushConstants, uint instanceIndex: SV_VulkanInstanceID)
{
    VSOutput output;
    RenderableData renderData = pushConstants.renderableData[pushConstants.renderableIndices[instanceIndex]];
    float4x4 modelMat4 = float4x4(
        renderData.transformRow0,
        renderData.transformRow1,
        renderData.transformRow2,
        float4(0.0f, 0.0f, 0.0f, 1.0f));

    float4 worldPos = mul(modelMat4, float4(input.Pos, 1.0f));

    output.Normal = mul((float3x3)mul(pushConstants.globalData->view, modelMat4), input.Normal);
    output.UV = input.UV;
    output.Pos = mul(pushConstants.globalData->projection, mul(pushConstants.globalData->view, worldPos));
    output.Factor = 1.0f;
    // Calculate view vectors required for lighting
    float4 fragPos = mul(pushConstants.globalData->view, worldPos);
    float4 lightViewPos = mul(pushConstants.globalData->view, pushConstants.globalData->lightPos);
    output.LightVec = lightViewPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;
    output.textureID = renderData.textureIDs_0_3.x; 
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
    
    return float4((diffuse + specular) * color.rgb, 1.0);
}
