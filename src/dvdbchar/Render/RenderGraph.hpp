#pragma once

#include <webgpu/webgpu_cpp.h>
#include <type_traits>
#include "Context.hpp"
#include "dvdbchar/Render/Context.hpp"

namespace dvdbchar::Render {
	template<typename T>
	concept Pass = requires(const T& t, wgpu::CommandEncoder& cmd) { t.execute(cmd); };

	struct ResourceRef {};

	struct TextureRef {
		size_t						 id;
		wgpu::TextureDescriptor		 desc;

		inline friend constexpr auto operator==(const TextureRef& lhs, const TextureRef& rhs) {
			return lhs.id == rhs.id;
		}
	};

	struct BufferRef {
		size_t						 id;
		wgpu::BufferDescriptor		 desc;

		inline friend constexpr auto operator==(const BufferRef& lhs, const BufferRef& rhs) {
			return lhs.id == rhs.id;
		}
	};

	template<Pass... PassTs>
	class RuntimeRenderGraph {
	public:
		RuntimeRenderGraph(
			std::tuple<PassTs...>&& ps, std::vector<size_t>&& ps_idx,
			std::vector<wgpu::TextureDescriptor>&& tex_descs,
			std::vector<wgpu::BufferDescriptor>&&  buf_descs
		) :
			_ps(std::move(ps)),
			_ps_idx(std::move(ps_idx)),
			_tex_descs(std::move(tex_descs)),
			_buf_descs(std::move(buf_descs)) {}

	public:
		auto allocate(const wgpu::Device& device) {
			_texs.reserve(_tex_descs.size());
			for (const auto& desc : _tex_descs) _texs.emplace_back(device.CreateTexture(&desc));
			_bufs.reserve(_buf_descs.size());
			for (const auto& desc : _buf_descs) _bufs.emplace_back(device.CreateBuffer(&desc));
		}

		auto execute(const wgpu::CommandEncoder& cmd) const {
			for (const auto idx : _ps_idx) std::get<idx>(_ps).execute(cmd);
		}

	private:
		std::tuple<PassTs...>				 _ps;
		std::vector<size_t>					 _ps_idx;
		std::vector<wgpu::TextureDescriptor> _tex_descs;
		std::vector<wgpu::BufferDescriptor>	 _buf_descs;
		std::vector<wgpu::Texture>			 _texs;
		std::vector<wgpu::Buffer>			 _bufs;
	};

	class RenderGraphBuilder {
	public:
		constexpr auto texture(const wgpu::TextureDescriptor& desc) -> TextureRef {
			return { texture_count++, desc };
		}

		inline static constexpr auto present_texture(const wgpu::TextureDescriptor& desc)
			-> TextureRef {
			return { static_cast<size_t>(-1), desc };
		}

		constexpr auto buffer(const wgpu::BufferDescriptor& desc) -> BufferRef {
			return { buffer_count++, desc };
		}

		template<typename... PassTs>
		auto build_runtime(PassTs&&... passes) const -> RuntimeRenderGraph<PassTs...> {
			// TODO:
		}

	private:
		size_t texture_count = 0;
		size_t buffer_count	 = 0;
	};

	struct BlitPass {
		TextureRef src;
		TextureRef dst;

		//
		auto execute(wgpu::CommandEncoder& cmd) const {}
	};

	class BasePass {
		TextureRef target;
		TextureRef depth;

		//
		auto execute(wgpu::CommandEncoder& cmd) const {}
	};

	class LightingPass {};

	inline static auto _test_render_graph() {
		auto builder = RenderGraphBuilder {};

		// clang-format off
		auto tex1 = builder.texture({
			.dimension 	= wgpu::TextureDimension::e2D,
			.size 		= wgpu::Extent3D { 800, 800 },
		});
		// clang-format on

		// auto graph =
		builder.build_runtime(	//
			BlitPass { tex1, tex1 }
		);
	}
}  // namespace dvdbchar::Render