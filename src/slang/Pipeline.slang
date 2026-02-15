module Pipeline;

import Utils;
import Uniform;
import Vertex;

public struct VertexOutput {
    public float4 sv_position : SV_Position;
    public float3 color;
	public float2 uv;
};

static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[4] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 0.5, 0.5)
);

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID , VertexInput input) {
    VertexOutput output;
    output.sv_position = mul(camera.projection_matrix, mul(camera.view_matrix, float4(input.pos, 1.)));
    // output.sv_position = mul(camera.projection_matrix, mul(pbr.camera.view_matrix, float4(input.pos, 1.)));
    // output.color = colors[vid];
	output.color = float3(.4, .8, .9);
	output.uv	 = input.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput in_vert) : SV_Target {
    // uint width, height;
	// pbr.tex_albedo.GetDimensions(width, height);
	// int2 pixel_coords = int2(in_vert.uv * float2(width, height));
	// return pbr.tex_albedo[pixel_coords];
    
	return pbr.tex_albedo.Sample(pbr.smp_albedo, in_vert.uv);
	// return float4(in_vert.uv, 0., 1.);
	// return float4(.1, .8, .9, 1.);
}