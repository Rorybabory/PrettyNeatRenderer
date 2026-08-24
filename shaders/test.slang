struct VSInput {
    float3 Pos;
    float3 Normal;
    float2 UV;
};


struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
    float4 lightPos;
    uint32_t selected;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShaderData> g_shaderData;

[[vk::binding(1, 0)]]
Sampler2D g_texture;

[shader("vertex")]
VSOutput main(VSInput input, uint instanceIndex : SV_VulkanInstanceID) 
{
    VSOutput output;
    float4x4 modelMat = g_shaderData.model[instanceIndex];
    output.Normal = mul((float3x3)mul(g_shaderData.view, modelMat), input.Normal);
    output.UV = input.UV;
    output.Pos = mul(g_shaderData.projection, mul(g_shaderData.view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
    output.Factor = 1.0f;
    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(g_shaderData.view, modelMat), float4(input.Pos.xyz, 1.0));
    output.LightVec = g_shaderData.lightPos.xyz - fragPos.xyz;
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
    float3 color = g_texture.Sample(input.UV).rgb * input.Factor;
    return float4(diffuse * color.rgb + specular, 1.0);
}
