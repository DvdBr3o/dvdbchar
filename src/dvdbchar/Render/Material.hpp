#pragma once

#include "dvdbchar/Render/Pipeline.hpp"

#include <webgpu/webgpu_cpp.h>
#include <range/v3/all.hpp>

#include <functional>

namespace dvdbchar::Render {
	struct Material {
		const Pipeline&				 pipeline;
		std::vector<wgpu::BindGroup> bindgroups;

		virtual auto bindings() -> std::vector<std::reference_wrapper<wgpu::BindGroup>> {
			return																	//
				bindgroups															//
				| ranges::views::transform([](auto&& bg) { return std::ref(bg); })	//
				| ranges::to<std::vector>();
		}
	};
}  // namespace dvdbchar::Render