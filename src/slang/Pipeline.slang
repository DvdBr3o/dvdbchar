module Pipeline;

import Utils;
import Uniform;
import Vertex;

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
    // output.sv_position = float4(input.pos, 1.);
    output.color = colors[vid];
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput in_vert) : SV_Target {
    float3 color = in_vert.color;
    // float3 color = camera.position.mul(.5 * sin(global.time) + .5);
    return float4(color, 1.0);
}