module Vertex;

public struct VertexInput {
	public float3 pos;
	public float3 normal;
	public float2 uv;
	public uint   tex_id;
};

public struct VertexOutput {
    public float4 sv_position : SV_Position;
    public float3 color;
};