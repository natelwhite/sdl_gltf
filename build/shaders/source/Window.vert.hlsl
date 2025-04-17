struct Input {
	uint VertexIndex : SV_VertexID;
};

struct Output {
	float2 TexCoord : TEXCOORD0;
	float4 Position : SV_Position;
};

Output main(Input input) {
	Output output;
	float2 pos;
	switch(input.VertexIndex) {
	case 0:
		pos = float2(-1.0f, 1.0f); // top left
		output.TexCoord = float2(0, 0);
		break;
	case 1:
		pos = float2(1.0f, 1.0f); // top right
		output.TexCoord = float2(1, 0);
		break;
	case 2:
		pos = float2(1.0f, -1.0f); // bottom right
		output.TexCoord = float2(1, 1);
		break;
	case 3:
		pos = float2(-1.0f, 1.0f); // top left
		output.TexCoord = float2(0, 0);
		break;
	case 4:
		pos = float2(1.0f, -1.0f); // bottom right
		output.TexCoord = float2(1, 1);
		break;
	case 5:
		pos = float2(-1.0f, -1.0f); // bottom left
		output.TexCoord = float2(0, 1);
		break;
	}
	output.Position = float4(pos, 0.0f, 1.0f);
	return output;
}
