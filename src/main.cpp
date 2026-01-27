#include "GLFW/glfw3.h"
#include "dvdbchar/OscServer.hpp"
// #include "dvdbchar/Forwarding.hpp"
#include "dvdbchar/Pipeline.hpp"
#include "dvdbchar/Model.hpp"
#include "dvdbchar/Render.hpp"
#include "dvdbchar/Render/Bindgroup.hpp"
#include "dvdbchar/Render/Buffer.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "exec/async_scope.hpp"
#include "exec/static_thread_pool.hpp"
#include "exec/task.hpp"
#include "simdjson.h"
#include "slang.h"
#include "webgpu/webgpu_cpp.h"

#include <asio.hpp>
#include <exception>
#include <fastgltf/core.hpp>

#include <thread>

using namespace dvdbchar;

// int main() {
// 	// GlfwManager::init();
// 	auto pipeline = Pipeline(
// 		{
// 			.width	= 1080,
// 			.height = 720,
// 			.title	= "dvdbchar",
// 		}
// 	);
// 	auto osc = OscService(
// 		{
// 			.port = 15784,
// 			.handler =
// 				[](const osc::ReceivedPacket& packet) {
// 					if (packet.IsBundle()) {
// 						const auto bundle = osc::ReceivedBundle(packet);
// 						//
// 					} else {  // packet.IsMessage()
// 						const auto message = osc::ReceivedMessage(packet);
// 						//
// 					}
// 				},
// 		}
// 	);
// 	std::jthread osc_thread([&]() { osc.serve(); });
// 	std::jthread render_thread([&]() {
// 		pipeline.launch();
// 		osc.stop();
// 	});

// 	// std::jthread load_model([&]() {
// 	// 	//
// 	// 	try {
// 	// 		auto model = Model("./mrweird.vrm");
// 	// 		model.introduce_self();
// 	// 	} catch (const std::exception& e) { spdlog::error(e.what()); }
// 	// 	spdlog::info("model loaded!");
// 	// });

// 	// std::jthread spout_forward([&]() {
// 	// 	try {
// 	// 		SpoutForwarder forwarder;
// 	// 		// forwarder.send({});
// 	// 	} catch (const std::exception& e) { spdlog::error(e.what()); }
// 	// });
// 	// std::jthread face_tracker([&]() {
// 	// 	system("cd trackers/mediapipe");
// 	// 	system("pixi run python tracker.py");
// 	// });
// }

using namespace stdexec;
using namespace dvdbchar::Render;

#include "dvdbchar/Render/Window.hpp"
#include "dvdbchar/Render/Pipeline.hpp"

inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
};

int main() {
	try {
		auto context = WgpuContext::global();
		auto pool	 = exec::static_thread_pool {};
		auto sched	 = context.get_scheduler_from(pool.get_scheduler());
		auto window	 = Window {
			 {
				 .width	 = 1920,
				 .height = 1080,
				 .title	 = "dvdbchar",
			  }
		};

		// const auto refl = get_mapping<Uniform>("shaders/Uniform.refl.json");
		//
		const auto vb = StaticVertexBuffer<Vertice> {
			// clang-format off
				to_span(
					std::array {
						Vertice { .pos = { .0, .5, 0. } },
						Vertice { .pos = { .5, -.5, .0 } },
						Vertice { .pos = { -.5, -.5, .0 } },
					}
				),
			// clang-format on
		};
		const auto camera  = get_mapping<CameraRefl>("camera", "shaders/Uniform.refl.json");

		const auto layouts = std::array {
			layout<GlobalRefl>("global", "shaders/Uniform.refl.json"),
			layout<CameraRefl>("camera", "shaders/Uniform.refl.json"),
			// layout<PbrRefl>("pbr", "shaders/Uniform.refl.json"),
			// layout<ModelDataRefl>("model_data", "shaders/Uniform.refl.json"),
		};

		// clang-format off
		auto pipeline = Pipeline {
			context,
			Pipeline::Spec {
				.shader			   = *read_text_from("shaders/Pipeline.wgsl"),
				.format			   = window.format(),
				.bindgroup_layouts = to_span(layouts), 
			}
		};
		// clang-format on

		const auto global	 = get_mapping<GlobalRefl>("global", "shaders/Uniform.refl.json");
		auto	   global_ub = ReflectedUniformBuffer<GlobalRefl> { global };
		auto	   global_bg = Bindgroup {
				  {
					  .layout = layout<GlobalRefl>("global", "shaders/Uniform.refl.json"),
					  .entries =
					  std::array {
						  wgpu::BindGroupEntry {
								  .binding = 0,
								  .buffer  = global_ub,
								  .offset  = global.offset,
								  .size	   = global.size,
						  },
					  }, }
		};

		auto camera_ub = ReflectedUniformBuffer<CameraRefl> { camera };
		camera_ub.write(camera.position, glm::vec3 { 0.6, 0.8, 0.9 });
		auto camera_bg = Bindgroup {
			{
				.layout = layout<CameraRefl>("camera", "shaders/Uniform.refl.json"),
				.entries =
					std::array {
						wgpu::BindGroupEntry {
							.binding = 0,
							.buffer	 = camera_ub,
							.offset	 = camera.offset,
							.size	 = camera.size,
						},
					}, }
		};

		spdlog::info("after bg");

		while (!glfwWindowShouldClose(window.window())) {
			glfwPollEvents();

			wgpu::SurfaceTexture tex;
			window.surface().GetCurrentTexture(&tex);

			global_ub.write(global.time, (float)glfwGetTime());

			const wgpu::RenderPassColorAttachment attachment {
				.view	 = tex.texture.CreateView(),
				.loadOp	 = wgpu::LoadOp::Clear,
				.storeOp = wgpu::StoreOp::Store,
			};

			const wgpu::RenderPassDescriptor renderpass {
				.colorAttachmentCount = 1,
				.colorAttachments	  = &attachment,
			};

			wgpu::CommandEncoder	encoder = context.device.CreateCommandEncoder();
			wgpu::RenderPassEncoder pass	= encoder.BeginRenderPass(&renderpass);
			pass.SetPipeline(pipeline.get());
			pass.SetVertexBuffer(0, vb.get());
			pass.SetBindGroup(0, global_bg);
			pass.SetBindGroup(1, camera_bg);
			pass.Draw(3);
			pass.End();
			wgpu::CommandBuffer commands = encoder.Finish();
			context.device.GetQueue().Submit(1, &commands);

			window.surface().Present();
			context.instance.ProcessEvents();
		}

	} catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
}