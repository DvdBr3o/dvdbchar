module Uniform;

public struct Viewport {
	public int32_t width;
	public int32_t height;
};

public struct Global {
	public float time;
	public float	frame;
	public Viewport viewport;
}

public struct Camera {
	public float3 			   position;
	public float3			   direction;
}

public struct PbrMaterial {
	public Texture2D basic_texture;
	public Texture2D normal_texture;
	public float4 base_color;
	public float  metallic;
	public float  roughness;
	public float  alpha_cutoff;
}

public struct ModelData {
	public float4x4 model_matrix;
}

public ParameterBlock<Global> global;
public ParameterBlock<Camera> camera;
// public ParameterBlock<PbrMaterial> pbr;
// public ParameterBlock<ModelData> model_data;
