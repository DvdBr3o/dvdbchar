#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/ShaderReflection.hpp"

#include <dawn/native/DawnNative.h>

#include <simdjson.h>

namespace dvdbchar::Render {
	class Bindgroup : public wgpu::BindGroup {
	public:
		struct Spec {
			wgpu::BindGroupLayout				  layout;
			std::span<const wgpu::BindGroupEntry> entries;
		};

	public:
		Bindgroup(const WgpuContext& ctx, const Spec& spec) {
			const wgpu::BindGroupDescriptor desc = {
				.layout		= spec.layout,
				.entryCount = spec.entries.size(),
				.entries	= spec.entries.data(),
			};
			static_cast<wgpu::BindGroup&>(*this) = ctx.device.CreateBindGroup(&desc);
		}

		Bindgroup(const Spec& spec) : Bindgroup(WgpuContext::global(), spec) {}

	public:
		[[nodiscard]] auto get() const -> const wgpu::BindGroup& { return *this; }
	};
}  // namespace dvdbchar::Render