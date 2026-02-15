#include "dvdbchar/MidiVideoPlayerApp.hpp"
#include "dvdbchar/Render.hpp"
#include "dvdbchar/Render/Camera.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Pass/Basepass.hpp"
#include "dvdbchar/Render/PipelineCache.hpp"
#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Render/RenderGraph.hpp"
#include "dvdbchar/Render/Window.hpp"
#include "dvdbchar/Model.hpp"
#include "dvdbchar/Utils.hpp"
#include "dvdbchar/VtubingApp.hpp"

inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
};

using namespace dvdbchar::Render;
using namespace dvdbchar;

int main2() {
	// try {
	_test_render_graph();
	// } catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
	return 0;
}

int main1() {
	try {
		auto model = Model { "public/VRM1_Constraint_Twist_Sample.vrm" };
		model.introduce_self();
	} catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
	return 0;
}

int main4() {
	spdlog::info("Welcome!");
	auto app = MidiVideoPlayerApp { { .video_path = "public/cod20.mp4" } };
	app.launch();
	return 0;
}

int main() {
	// try {
	auto app = VtubingApp {{
			.window = {
				.width	     = 1920,
				.height	     = 1080,
				.title	     = "你好我是DvdBr3o",
				.transparent = true,
			},
			.model = Model { "public/VRM1_Constraint_Twist_Sample.vrm" },
		}};
	app.launch();
	// } catch (const std::exception& e) { spdlog::critical("Uncaught exception: {}", e.what()); }
	return 0;
}

int main3() {
	// try {
	auto& context = WgpuContext::global();

	//
	auto cam = Camera {
		.position  = { 0., 0.,  1. },
		.direction = { 0., 0., -2. },
	};

	auto global_ub = ReflectedUniformBuffer<GlobalRefl> {
		get_mapping<GlobalRefl>("global", "shaders/Uniform.refl.json")
	};
	// clang-format off
	auto global_bg = Bindgroup {{
		.layout = parsed::bindgroup_layout_from_path("global", "shaders/Uniform.refl.json"),
		.entries = std::array { wgpu::BindGroupEntry {
			.binding = 0,
			.buffer  = global_ub,
			.offset  = global_ub.offset,
			.size	 = global_ub.size,
		}}, 
	}};
	// clang-format on

	auto camera_ub = ReflectedUniformBuffer<CameraRefl> {
		get_mapping<CameraRefl>("camera", "shaders/Uniform.refl.json")
	};
	camera_ub.write(camera_ub.view_matrix, cam.view_matrix());
	camera_ub.write(camera_ub.projection_matrix, cam.projection_matrix());
	// clang-format off
	auto camera_bg = Bindgroup {{
		.layout	 = parsed::bindgroup_layout_from_path("global", "shaders/Uniform.refl.json"),
		.entries = std::array { wgpu::BindGroupEntry {
			.binding = 0,
			.buffer	 = camera_ub,
			.offset	 = camera_ub.offset,
			.size	 = camera_ub.size,
		}},
	}};
	// clang-format on

	auto window = Window {
		{
			.width		 = 1920,
			.height		 = 1080,
			.title		 = "dvdbchar",
			.transparent = true,
		 }
	};
	cam.aspect = window.aspect();

	std::mutex mutex;

	auto	   window_activity = window.bind(
		  FpsCameraController { cam },
		  Window::EscExiter { window },
		  CameraAspectAdaptor { cam },
		  [&](Window::on_mouse_moved_t, auto&&...) {
			  std::unique_lock lock { mutex };
			  camera_ub.write(camera_ub.view_matrix, cam.view_matrix());
			  camera_ub.write(camera_ub.projection_matrix, cam.projection_matrix());
		  }
	  );

	auto vb = ArrayVertexBuffer<Vertice> {
		std::array {
					Vertice { .pos = { .5, .5, 0. } },
					Vertice { .pos = { .5, -.5, .0 } },
					Vertice { .pos = { -.5, -.5, .0 } },
					Vertice { .pos = { -.5, .5, .0 } },
					}
	};
	auto ib = ArrayIndexBuffer {
		std::array { 0u, 1u, 2u, 0u, 2u, 3u }
	};

	// clang-format off
	auto pipeline = Pipeline {{
		.shader = *read_text_from("shaders/Pipeline.wgsl"),
		.reflection = *read_text_from("shaders/Uniform.layout.json"),
		.format = window.format(),
	}};
	// clang-format on

	auto mesh = MeshPrimitive {
		.buf_vertex		 = vb,
		.buf_index		 = ib,
		.buf_index_count = 6,
	};

	std::jthread render { [&]() {
		while (!glfwWindowShouldClose(window.window())) {
			wgpu::SurfaceTexture tex;
			window.surface().GetCurrentTexture(&tex);

			std::unique_lock lock { mutex };

			//
			// auto base = Pass::basepass(tex.texture, {
			// 	.mesh = mesh,
			// 	.pipeline = pipeline,
			// 	.bindgroups = {
			// 		global_bg,
			// 		camera_bg,
			// 	},
			// });
			// context.queue.Submit(1, &base);

			window.surface().Present();
			context.instance.ProcessEvents();
		}
	} };

	while (!glfwWindowShouldClose(window.window())) glfwPollEvents();

	// } catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
	return 0;
}