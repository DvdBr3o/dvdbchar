#pragma once

#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Window.hpp"

#include <webgpu/webgpu_cpp.h>

namespace dvdbchar::Render {
	struct TextureWrite {
		wgpu::Texture texture;
		wgpu::LoadOp  load	= wgpu::LoadOp::Clear;
		wgpu::StoreOp store = wgpu::StoreOp::Store;
	};

	struct ImageInfo {
		std::span<char> data;
		uint32_t		width;
		uint32_t		height;
	};

	inline auto texture_from_image(const WgpuContext& ctx, const ImageInfo& image)
		-> wgpu::Texture {
		const wgpu::TextureDescriptor desc {
			.usage		   = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
			.dimension	   = wgpu::TextureDimension::e2D,
			.size		   = { image.width, image.height, 1 },
			.format		   = wgpu::TextureFormat::RGBA8Unorm,
			.mipLevelCount = 1,
			.sampleCount   = 1,
		};
		wgpu::Texture					 texture = ctx.device.CreateTexture(&desc);

		const wgpu::TexelCopyTextureInfo dest {
			.texture  = texture,
			.mipLevel = 0,
			.aspect	  = wgpu::TextureAspect::All,
		};
		const wgpu::TexelCopyBufferLayout layout {
			.offset		  = 0,
			.bytesPerRow  = image.width * 4,
			.rowsPerImage = image.height,
		};
		ctx.queue.WriteTexture(
			&dest,
			image.data.data(),
			image.width * image.height * 4,
			&layout,
			&desc.size
		);

		return texture;
	}

	inline auto texture_from_image(const ImageInfo& image) -> wgpu::Texture {
		return texture_from_image(WgpuContext::global(), image);
	}

	inline auto depth_texture(const WgpuContext& ctx, const Size& size) -> wgpu::Texture {
		const wgpu::TextureFormat	  format = wgpu::TextureFormat::Depth24Plus;
		const wgpu::TextureDescriptor desc {
			.usage	   = wgpu::TextureUsage::RenderAttachment,
			.dimension = wgpu::TextureDimension::e2D,
			.size	= { static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height), 1 },
			.format = format,
			.viewFormatCount = 1,
			.viewFormats	 = &format,
		};
		return ctx.device.CreateTexture(&desc);
	}

	inline auto depth_texture(const Size& size) -> wgpu::Texture {
		return depth_texture(WgpuContext::global(), size);
	}

	inline auto linear_repeat_sampler(const WgpuContext& ctx) -> wgpu::Sampler {
		const wgpu::SamplerDescriptor desc {
			.addressModeU = wgpu::AddressMode::Repeat,
			.addressModeV = wgpu::AddressMode::Repeat,
			.addressModeW = wgpu::AddressMode::Repeat,
			.magFilter	  = wgpu::FilterMode::Linear,
			.minFilter	  = wgpu::FilterMode::Linear,
		};
		return ctx.device.CreateSampler(&desc);
	}

	inline auto linear_repeat_sampler() -> wgpu::Sampler {
		return linear_repeat_sampler(WgpuContext::global());
	}

	inline auto isotropic_sampler(
		const WgpuContext& ctx, wgpu::AddressMode address_mode, wgpu::FilterMode filter
	) -> wgpu::Sampler {
		const wgpu::SamplerDescriptor desc {
			.addressModeU = address_mode,
			.addressModeV = address_mode,
			.addressModeW = address_mode,
			.magFilter	  = filter,
			.minFilter	  = filter,
		};
		return ctx.device.CreateSampler(&desc);
	}

	inline auto isotropic_sampler(wgpu::AddressMode address_mode, wgpu::FilterMode filter)
		-> wgpu::Sampler {
		return isotropic_sampler(WgpuContext::global(), address_mode, filter);
	}

	class ScreenwiseTextureManager {
	public:
		// TODO:

	public:
		auto operator()(const Window::on_window_resize_t, const Size& size) {
			for (auto& tex : _textures);  // TODO:
		}

	private:
		std::vector<wgpu::Texture> _textures;
	};
}  // namespace dvdbchar::Render