#pragma once

#include "dvdbchar/Render/ShaderReflection.hpp"

#include <cstdint>

namespace dvdbchar::Render {
	struct Size {
		int							 width;
		int							 height;

		[[nodiscard]] constexpr auto aspect() const -> float {
			return static_cast<float>(width) / static_cast<float>(height);
		}
	};

	struct Pos {
		int x;
		int y;
	};

	struct CursorOffset {
		double dx;
		double dy;
	};

	struct KeySignal {
		int key;
		int scancode;
		int action;
		int mods;
	};

	struct ViewportRefl {
		Field<int32_t> width;
		Field<int32_t> height;
	};

	template<>
	struct ReflectionRegistry<ViewportRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {	"width",	 &ViewportRefl::width },
				std::pair { "height", &ViewportRefl::height },
			};
		}
	};

	struct GlobalRefl {
		Field<float>		time;
		Field<float>		frame;
		Field<ViewportRefl> viewport;
	};

	template<>
	struct ReflectionRegistry<GlobalRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {		"time",		&GlobalRefl::time },
				std::pair {	"frame",	 &GlobalRefl::frame },
				std::pair { "viewport", &GlobalRefl::viewport },
			};
		}
	};

	struct CameraRefl {
		Field<glm::mat4x4> view_matrix;
		Field<glm::mat4x4> projection_matrix;
	};

	template<>
	struct ReflectionRegistry<CameraRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {		"view_matrix",	   &CameraRefl::view_matrix },
				std::pair { "projection_matrix", &CameraRefl::projection_matrix },
			};
		}
	};

	struct PbrRefl {
		Field<glm::vec4> tex_albedo;
		Field<glm::vec4> smp_albedo;
		Field<float>	 metallic;
		// Field<float>	 roughness;
		// Field<float>	 alpha_cutoff;
	};

	template<>
	struct ReflectionRegistry<PbrRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair { "tex_albedo", &PbrRefl::tex_albedo },
				std::pair { "smp_albedo", &PbrRefl::smp_albedo },
				std::pair {	"metallic",	&PbrRefl::metallic },
				// std::pair {	"roughness",	 &PbrRefl::roughness },
				// std::pair { "alpha_cutoff", &PbrRefl::alpha_cutoff },
			};
		}
	};

	struct ModelDataRefl {
		Field<glm::mat4x4> model_matrix;
	};

	template<>
	struct ReflectionRegistry<ModelDataRefl> {
		inline static consteval auto mapping() {
			return std::make_tuple(	 //
				std::pair { "model_matrix", &ModelDataRefl::model_matrix }
			);
		}
	};

	struct Uniform {
		ReflectedParameter<CameraRefl>	  camera;
		ReflectedParameter<PbrRefl>		  pbr;
		ReflectedParameter<ModelDataRefl> model_data;
	};

	template<>
	struct ParameterRegistry<Uniform> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {		"camera",	  &Uniform::camera },
				std::pair {		"pbr",		   &Uniform::pbr },
				std::pair { "model_data", &Uniform::model_data },
			};
		}
	};

}  // namespace dvdbchar::Render
