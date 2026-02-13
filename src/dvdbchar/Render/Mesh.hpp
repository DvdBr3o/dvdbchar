#pragma once

#include "dvdbchar/Render/Pipeline.hpp"

#include <webgpu/webgpu_cpp.h>

namespace dvdbchar::Render {
	struct MeshPrimitive {
		wgpu::Buffer			 buf_vertex;
		wgpu::VertexBufferLayout buf_vertex_layout = Render::vertex_layout<Vertice>();
		wgpu::Buffer			 buf_index;
		size_t					 buf_index_count  = 0;
		wgpu::IndexFormat		 buf_index_format = wgpu::IndexFormat::Uint32;

		wgpu::BindGroup			 bg_pbr;
		wgpu::Buffer			 buf_pbr;
		wgpu::Texture			 tex_albedo;
	};
}  // namespace dvdbchar::Render