struct Camera_std140_0
{
    @align(16) position_0 : vec3<f32>,
    @align(16) direction_0 : vec3<f32>,
};

@binding(0) @group(1) var<uniform> camera_0 : Camera_std140_0;
struct Viewport_std140_0
{
    @align(16) width_0 : i32,
    @align(4) height_0 : i32,
};

struct Global_std140_0
{
    @align(16) time_0 : f32,
    @align(4) frame_0 : f32,
    @align(16) viewport_0 : Viewport_std140_0,
};

@binding(0) @group(0) var<uniform> global_0 : Global_std140_0;
fn _S1( _S2 : ptr<function, array<vec3<f32>, i32(3)>>)
{
    const _S3 : vec3<f32> = vec3<f32>(0.0f, 1.0f, 0.0f);
    const _S4 : vec3<f32> = vec3<f32>(0.0f, 0.0f, 1.0f);
    (*_S2)[i32(0)] = vec3<f32>(1.0f, 0.0f, 0.0f);
    (*_S2)[i32(1)] = _S3;
    (*_S2)[i32(2)] = _S4;
    return;
}

var<private> colors_0 : array<vec3<f32>, i32(3)>;

struct VertexOutput_0
{
    @builtin(position) sv_position_0 : vec4<f32>,
    @location(0) color_0 : vec3<f32>,
};

struct vertexInput_0
{
    @location(0) pos_0 : vec3<f32>,
    @location(1) normal_0 : vec3<f32>,
    @location(2) uv_0 : vec2<f32>,
    @location(3) tex_id_0 : u32,
};

@vertex
fn vertMain( _S5 : vertexInput_0, @builtin(vertex_index) vid_0 : u32) -> VertexOutput_0
{
    var _S6 : array<vec3<f32>, i32(3)>;
    _S1(&(_S6));
    colors_0 = _S6;
    var output_0 : VertexOutput_0;
    output_0.sv_position_0 = vec4<f32>(_S5.pos_0, 1.0f);
    output_0.color_0 = colors_0[vid_0];
    return output_0;
}

struct pixelOutput_0
{
    @location(0) output_1 : vec4<f32>,
};

struct pixelInput_0
{
    @location(0) color_1 : vec3<f32>,
};

@fragment
fn fragMain( _S7 : pixelInput_0, @builtin(position) sv_position_1 : vec4<f32>) -> pixelOutput_0
{
    var _S8 : array<vec3<f32>, i32(3)>;
    _S1(&(_S8));
    colors_0 = _S8;
    var _S9 : pixelOutput_0 = pixelOutput_0( vec4<f32>(camera_0.position_0 * vec3<f32>((0.5f * sin(global_0.time_0) + 0.5f)), 1.0f) );
    return _S9;
}

