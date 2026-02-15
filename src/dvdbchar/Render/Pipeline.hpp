#pragma once

#include "ShaderReflection.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Bindgroup.hpp"
#include "dvdbchar/Utils.hpp"
#include "webgpu/webgpu_cpp.h"

#include <dawn/webgpu_cpp.h>
#include <cstddef>
#include <glm/glm.hpp>

#include <span>

namespace dvdbchar::Render {
	struct Vertice {
		glm::vec3					 pos;
		glm::vec3					 normal;
		glm::vec2					 uv;
		size_t						 tex_id;

		inline static constexpr auto vertex_attribute() noexcept {
			return std::to_array<wgpu::VertexAttribute>({
				{
					.format			= wgpu::VertexFormat::Float32x3,
					.offset			= offsetof(Vertice,	pos),
					.shaderLocation = 0,
				 },
				{
					.format			= wgpu::VertexFormat::Float32x3,
					.offset			= offsetof(Vertice, normal),
					.shaderLocation = 1,
				 },
				{
					.format			= wgpu::VertexFormat::Float32x2,
					.offset			= offsetof(Vertice,		uv),
					.shaderLocation = 2,
				 },
				{
					.format			= wgpu::VertexFormat::Uint32,
					.offset			= offsetof(Vertice, tex_id),
					.shaderLocation = 3,
				 },
			});
		}
	};

	template<typename T>
		requires requires {
			{ T::vertex_attribute() } -> FatPointerAlike;
		}
	inline constexpr auto vertex_attribute = T::vertex_attribute();

	//
	template<typename T>
		requires requires {
			{ T::vertex_attribute() } -> FatPointerAlike;
		} && ConstEvaluated<T::vertex_attribute()>
	inline constexpr auto vertex_layout() -> wgpu::VertexBufferLayout {
		return {
			.arrayStride	= sizeof(T),
			.attributeCount = vertex_attribute<T>.size(),
			.attributes		= vertex_attribute<T>.data(),
		};
	}

	class Pipeline : public wgpu::RenderPipeline {
	public:
		struct VertexInfo {
			size_t								   stride;
			std::span<const wgpu::VertexAttribute> attributes;
		};

		struct Spec {
			std::string_view	shader;
			std::string_view	reflection;
			wgpu::TextureFormat format;
			VertexInfo			vertex = []() -> VertexInfo {
				 static auto default_attrib = Vertice::vertex_attribute();
				 return {
							 .stride	 = sizeof(Vertice),
							 .attributes = default_attrib,
				 };
			}();
		};

	public:
		Pipeline(
			const WgpuContext& ctx,
			// clang-format off
			const Spec& spec = {
                .vertex = {
                    .stride	   = sizeof(Vertice),
				    .attributes = to_span(Vertice::vertex_attribute()),
				}
		    }	// clang-format on
		) {
			const wgpu::ShaderSourceWGSL	   wgsl { { .code = spec.shader } };

			const wgpu::ShaderModuleDescriptor shader_module_desc = { .nextInChain = &wgsl };
			const wgpu::ShaderModule		   shader_module =
				ctx.device.CreateShaderModule(&shader_module_desc);

			const wgpu::BlendState blend_state {
				.color = {
					
				},
				.alpha = {
					
				},
			};
			const wgpu::ColorTargetState color_target_state = {
				.format = spec.format,
				// .blend	= &blend_state,
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
			const auto bgls = parsed::bindgroup_layouts_from_string(ctx, spec.reflection);
			const wgpu::DepthStencilState depth_stencil_state {
				.format			   = wgpu::TextureFormat::Depth24Plus,
				.depthWriteEnabled = true,
				.depthCompare	   = wgpu::CompareFunction::Less,
			};
			const wgpu::RenderPipelineDescriptor pipeline_desc = {
				.layout = [&](){
					const wgpu::PipelineLayoutDescriptor desc = {
						.bindGroupLayoutCount = bgls.size(),
						.bindGroupLayouts = bgls.data(),
					};
					return ctx.device.CreatePipelineLayout(&desc);
				}(),
				.vertex	  = { 
					.module = shader_module, 
					.bufferCount = 1, 
					.buffers = &vertex_layout, 
				},
				.primitive = {
					.cullMode = wgpu::CullMode::None,
				},
				.depthStencil = &depth_stencil_state,
				.fragment = &fragment_state,
			};
			static_cast<wgpu::RenderPipeline&>(*this) =
				ctx.device.CreateRenderPipeline(&pipeline_desc);
		}

		Pipeline(
			// clang-format off
			const Spec& spec = {
                .vertex = {
                    .stride	   = sizeof(Vertice),
				    .attributes = to_span(Vertice::vertex_attribute()),
				}
		    }	// clang-format on
		) : Pipeline(WgpuContext::global(), spec) {}

	public:
		[[nodiscard]] auto get() const -> const wgpu::RenderPipeline& { return *this; }
	};
}  // namespace dvdbchar::Render