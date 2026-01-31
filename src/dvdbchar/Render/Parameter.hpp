#pragma once

#include "dvdbchar/Render/Bindgroup.hpp"

#include <simdjson.h>

namespace dvdbchar::Render {
	template<ReflMapped T>
	class Parameter : public ReflectedParameter<T> {
	public:
		Parameter(const WgpuContext& ctx, std::string_view name, simdjson::ondemand::value&& json) :
			ReflectedParameter<T>(get_mapping<T>(name, std::move(json))) {
			//
		}

	private:
		std::optional<wgpu::Buffer> _uniform;
		// std::vector<wgpu::Texture> _texs;
		Bindgroup _bindgroup;
	};
}  // namespace dvdbchar::Render
