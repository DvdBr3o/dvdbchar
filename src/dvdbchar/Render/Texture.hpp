#pragma once

#include "dvdbchar/Render/Context.hpp"

#include <webgpu/webgpu_cpp.h>

namespace dvdbchar::Render {
	class Texture : public wgpu::Texture {
	public:
		Texture(const WgpuContext& ctx) {
			const wgpu::TextureDescriptor tex_desc = {

			};
			static_cast<wgpu::Texture&>(*this) = ctx.device.CreateTexture(&tex_desc);
		}
	};
}  // namespace dvdbchar::Render