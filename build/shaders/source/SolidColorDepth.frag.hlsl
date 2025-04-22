cbuffer UBO : register(b0, space3) {
    float NearPlane;
    float FarPlane;
};

struct Output {
    float4 Color : SV_Target0;
    float Depth : SV_Depth;
};

float LinearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0;
    return ((2.0 * near * far) / (far + near - z * (far - near))) / far;
}

Output main(float3 Normal : NORMAL, float4 Position : SV_Position) {
	float ambient_strength = 0.1;
	float light_intensity = 1.0;
	float3 base_color = float3(1.0f, 1.0f, 1.0f);
	float3 light_direction = float3(1.0f, 1.0f, 1.0f);
	float3 surface_alignment = dot(normalize(light_direction), Normal);
	float3 diffuse_strength = light_intensity * surface_alignment;
	float3 result_color = base_color * (ambient_strength + diffuse_strength);
    Output result;
    result.Color = float4(Normal * (diffuse_strength + ambient_strength), 1.0f);
    result.Depth = LinearizeDepth(Position.z, NearPlane, FarPlane);
    return result;
}
