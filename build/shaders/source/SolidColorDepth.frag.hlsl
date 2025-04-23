cbuffer UBO : register(b0, space3) {
    float2 near_far;
	float3 view_pos;
};

struct Output {
    float4 Color : SV_Target0;
    float Depth : SV_Depth;
};
struct Input {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float4 WorldPos : TEXCOORD1;
};

float LinearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0;
    return ((2.0 * near * far) / (far + near - z * (far - near))) / far;
}

Output main(Input input) {
	float3 light_color = float3(1, 1, 1);
	float3 light_pos = float3(0, 100, 50);

	float3 ambient = 0.1 * light_color;

	float3 light_direction = normalize(light_pos - input.WorldPos.xyz);
	float3 diffuse = max(0, dot(light_direction, input.Normal)) * light_color;

	float3 view_direction = normalize(view_pos - input.WorldPos.xyz);
	float3 reflect_direction = reflect(-light_direction, input.Normal);
	float3 specular = pow(max(dot(view_direction, reflect_direction), 0.0), 32) * 0.5 * light_color;

	float3 obj_color = float3(0.6f, 0.3f, 0.2f);
    Output result;
    result.Color = float4(obj_color * (ambient + diffuse + specular), 1.0f);
    result.Depth = LinearizeDepth(input.Position.z, near_far.x, near_far.y);
    return result;
}
