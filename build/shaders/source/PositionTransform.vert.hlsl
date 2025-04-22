cbuffer UBO : register(b0, space1) {
    float4x4 proj_view;
    float4x4 model;
};

struct Input
{
    float3 Position : SV_Position;
    float3 Normal : NORMAL;
};

struct Output
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(mul(proj_view, model), float4(input.Position, 1.0f));
    output.Normal = normalize(mul((float3x3)model, input.Normal));
    return output;
}
