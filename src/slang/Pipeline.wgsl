struct _MatrixStorage_float4x4_ColMajorstd140_0
{
    @align(16) data_0 : array<vec4<f32>, i32(4)>,
};

struct Camera_std140_0
{
    @align(16) view_matrix_0 : _MatrixStorage_float4x4_ColMajorstd140_0,
    @align(16) projection_matrix_0 : _MatrixStorage_float4x4_ColMajorstd140_0,
};

@binding(0) @group(1) var<uniform> camera_0 : Camera_std140_0;
fn _S1( _S2 : ptr<function, array<vec3<f32>, i32(4)>>)
{
    const _S3 : vec3<f32> = vec3<f32>(0.0f, 1.0f, 0.0f);
    const _S4 : vec3<f32> = vec3<f32>(0.0f, 0.0f, 1.0f);
    const _S5 : vec3<f32> = vec3<f32>(0.0f, 0.5f, 0.5f);
    (*_S2)[i32(0)] = vec3<f32>(1.0f, 0.0f, 0.0f);
    (*_S2)[i32(1)] = _S3;
    (*_S2)[i32(2)] = _S4;
    (*_S2)[i32(3)] = _S5;
    return;
}

var<private> colors_0 : array<vec3<f32>, i32(4)>;

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
fn vertMain( _S6 : vertexInput_0, @builtin(vertex_index) vid_0 : u32) -> VertexOutput_0
{
    var _S7 : array<vec3<f32>, i32(4)>;
    _S1(&(_S7));
    colors_0 = _S7;
    var output_0 : VertexOutput_0;
    output_0.sv_position_0 = ((((((vec4<f32>(_S6.pos_0, 1.0f)) * (mat4x4<f32>(camera_0.view_matrix_0.data_0[i32(0)][i32(0)], camera_0.view_matrix_0.data_0[i32(1)][i32(0)], camera_0.view_matrix_0.data_0[i32(2)][i32(0)], camera_0.view_matrix_0.data_0[i32(3)][i32(0)], camera_0.view_matrix_0.data_0[i32(0)][i32(1)], camera_0.view_matrix_0.data_0[i32(1)][i32(1)], camera_0.view_matrix_0.data_0[i32(2)][i32(1)], camera_0.view_matrix_0.data_0[i32(3)][i32(1)], camera_0.view_matrix_0.data_0[i32(0)][i32(2)], camera_0.view_matrix_0.data_0[i32(1)][i32(2)], camera_0.view_matrix_0.data_0[i32(2)][i32(2)], camera_0.view_matrix_0.data_0[i32(3)][i32(2)], camera_0.view_matrix_0.data_0[i32(0)][i32(3)], camera_0.view_matrix_0.data_0[i32(1)][i32(3)], camera_0.view_matrix_0.data_0[i32(2)][i32(3)], camera_0.view_matrix_0.data_0[i32(3)][i32(3)]))))) * (mat4x4<f32>(camera_0.projection_matrix_0.data_0[i32(0)][i32(0)], camera_0.projection_matrix_0.data_0[i32(1)][i32(0)], camera_0.projection_matrix_0.data_0[i32(2)][i32(0)], camera_0.projection_matrix_0.data_0[i32(3)][i32(0)], camera_0.projection_matrix_0.data_0[i32(0)][i32(1)], camera_0.projection_matrix_0.data_0[i32(1)][i32(1)], camera_0.projection_matrix_0.data_0[i32(2)][i32(1)], camera_0.projection_matrix_0.data_0[i32(3)][i32(1)], camera_0.projection_matrix_0.data_0[i32(0)][i32(2)], camera_0.projection_matrix_0.data_0[i32(1)][i32(2)], camera_0.projection_matrix_0.data_0[i32(2)][i32(2)], camera_0.projection_matrix_0.data_0[i32(3)][i32(2)], camera_0.projection_matrix_0.data_0[i32(0)][i32(3)], camera_0.projection_matrix_0.data_0[i32(1)][i32(3)], camera_0.projection_matrix_0.data_0[i32(2)][i32(3)], camera_0.projection_matrix_0.data_0[i32(3)][i32(3)]))));
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
fn fragMain( _S8 : pixelInput_0, @builtin(position) sv_position_1 : vec4<f32>) -> pixelOutput_0
{
    var _S9 : array<vec3<f32>, i32(4)>;
    _S1(&(_S9));
    colors_0 = _S9;
    var _S10 : pixelOutput_0 = pixelOutput_0( vec4<f32>(_S8.color_1, 1.0f) );
    return _S10;
}

