Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

Texture2D DepthTexture : register(t1, space2);
SamplerState DepthSampler : register(s1, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    // get our color & depth value
    float4 color = ColorTexture.Sample(ColorSampler, TexCoord);
    float depth = 1000 - DepthTexture.Sample(DepthSampler, TexCoord).r;
	float depth_fade = saturate((abs(pow(depth, 1))) / 1);
    // combine results
	float4 result = color * depth_fade;
    return result;
}
