#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Mesh.hpp"
#include "dvdbchar/Render/Texture.hpp"

#include <webgpu/webgpu_cpp.h>
#include <range/v3/all.hpp>

namespace dvdbchar::Render::Pass {
	struct BasePass {
		TextureWrite tex_target;
		TextureWrite tex_depth;

		struct Executable {
			wgpu::CommandEncoder&	cmd;
			wgpu::RenderPassEncoder pass;

			//
			auto execute(
				const MeshPrimitive& mesh, const Pipeline& pipeline,
				const std::vector<wgpu::BindGroup>& bindgroups
			) const {
				pass.SetVertexBuffer(0, mesh.buf_vertex);
				pass.SetIndexBuffer(mesh.buf_index, mesh.buf_index_format);
				pass.SetPipeline(pipeline);
				for (auto [i, bg] : ranges::views::enumerate(bindgroups)) pass.SetBindGroup(i, bg);
				pass.DrawIndexed(mesh.buf_index_count);
			}

			[[nodiscard]] auto end() const {
				pass.End();
				return cmd.Finish();
			}
		};

		auto start(wgpu::CommandEncoder& cmd) const -> Executable {
			const wgpu::RenderPassColorAttachment color_attachment {
				.view	 = tex_target.texture.CreateView(),
				.loadOp	 = tex_target.load,
				.storeOp = tex_target.store,
			};
			const wgpu::RenderPassDepthStencilAttachment depth_attachment {
				.view			 = tex_depth.texture.CreateView(),
				.depthLoadOp	 = tex_depth.load,
				.depthStoreOp	 = tex_depth.store,
				.depthClearValue = 1.f,
			};
			const wgpu::RenderPassDescriptor desc {
				.colorAttachmentCount	= 1,
				.colorAttachments		= &color_attachment,
				.depthStencilAttachment = &depth_attachment,
			};
			return { cmd, cmd.BeginRenderPass(&desc) };
		}
	};
}  // namespace dvdbchar::Render::Pass