module Uniform;

struct UniformTagKind {
	static const string global = "global";
	static const string camera = "camera";
	static const string pbr	   = "pbr";
};

[__AttributeUsage(_AttributeTargets.Var)]
struct UniformTag {
	string kind;
};

public struct Viewport {
	public int32_t width;
	public int32_t height;
};

public struct Global {
	public float time;
	public float delta_time;
	public float	frame;
	public Viewport viewport;
}

public struct Camera {
	public float4x4 view_matrix;
	public float4x4 projection_matrix;
}

public struct PbrMaterial {
	public Texture2D	tex_albedo;
	public SamplerState			  smp_albedo;
	// public ParameterBlock<Camera> camera;
	// public float				  foo;
	// public Texture2D normal_texture;
	// public float4 base_color;
	// public float  metallic;
	// public float  roughness;
	// public float  alpha_cutoff;
}

public struct ModelData {
	public float4x4 model_matrix;
}

[UniformTag("global")]
public ParameterBlock<Global> global;
public ParameterBlock<Camera> camera;
public ParameterBlock<PbrMaterial> pbr;
