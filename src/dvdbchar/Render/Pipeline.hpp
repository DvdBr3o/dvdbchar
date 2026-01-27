#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Bindgroup.hpp"

#include <dawn/webgpu_cpp.h>
#include <glm/glm.hpp>

#include <span>

namespace dvdbchar::Render {
	struct Vertice {
		glm::vec3					 pos;
		glm::vec3					 normal;
		glm::vec2					 uv;
		size_t						 tex_id;

		inline static constexpr auto vertex_attrib() noexcept {
			return std::to_array<wgpu::VertexAttribute>({
				{
					.format			= wgpu::VertexFormat::Float32x3,
					.offset			= 0,
					.shaderLocation = 0,
				 },
				{
					.format			= wgpu::VertexFormat::Float32x3,
					.offset			= 0,
					.shaderLocation = 1,
				 },
				{
					.format			= wgpu::VertexFormat::Float32x2,
					.offset			= 0,
					.shaderLocation = 2,
				 },
				{
					.format			= wgpu::VertexFormat::Uint32,
					.offset			= 0,
					.shaderLocation = 3,
				 },
			});
		}
	};

	class Pipeline : public wgpu::RenderPipeline {
	public:
		struct VertexInfo {
			size_t								   stride;
			std::span<const wgpu::VertexAttribute> attributes;
		};

		struct Spec {
			std::string_view	shader;
			wgpu::TextureFormat format;
			VertexInfo			vertex = []() -> VertexInfo {
				 static auto default_attrib = Vertice::vertex_attrib();
				 return {
							 .stride	 = sizeof(Vertice),
							 .attributes = default_attrib,
				 };
			}();
			std::span<const wgpu::BindGroupLayout> bindgroup_layouts;
		};

	public:
		Pipeline(
			const WgpuContext& ctx,
			// clang-format off
			const Spec&		   spec = {
                .vertex = {
                    .stride	   = sizeof(Vertice),
				    .attributes = to_span(Vertice::vertex_attrib()),
				}
		    }	// clang-format on
		) {
			const wgpu::ShaderSourceWGSL	   wgsl { { .code = spec.shader } };

			const wgpu::ShaderModuleDescriptor shader_module_desc = { .nextInChain = &wgsl };
			const wgpu::ShaderModule		   shader_module =
				ctx.device.CreateShaderModule(&shader_module_desc);

			const wgpu::ColorTargetState color_target_state = {
				.format = spec.format,
			};
			const wgpu::FragmentState fragment_state = {
				.module		 = shader_module,
				.targetCount = 1,
				.targets	 = &color_target_state,
			};

			const wgpu::VertexBufferLayout vertex_layout = {
				.stepMode		= wgpu::VertexStepMode::Vertex,
				.arrayStride	= spec.vertex.stride,
				.attributeCount = spec.vertex.attributes.size(),
				.attributes		= spec.vertex.attributes.data(),
			};

			const wgpu::RenderPipelineDescriptor pipeline_desc = {
				.layout = [&](){
					const wgpu::PipelineLayoutDescriptor desc = {
						.bindGroupLayoutCount = spec.bindgroup_layouts.size(),
						.bindGroupLayouts = spec.bindgroup_layouts.data(),
					};
					return ctx.device.CreatePipelineLayout(&desc);
				}(),
				.vertex	  = { 
					.module = shader_module, 
					.bufferCount = 1, 
					.buffers = &vertex_layout, 
				},
				.fragment = &fragment_state,
			};
			spdlog::info("pre create");
			static_cast<wgpu::RenderPipeline&>(*this) =
				ctx.device.CreateRenderPipeline(&pipeline_desc);
		}

	public:

		[[nodiscard]] auto get() const -> const wgpu::RenderPipeline& { return *this; }
	};
}  // namespace dvdbchar::Render