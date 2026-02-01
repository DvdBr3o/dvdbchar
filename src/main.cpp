#include "GLFW/glfw3.h"
#include "dvdbchar/Render.hpp"
#include "dvdbchar/Render/Camera.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Render/RenderGraph.hpp"
#include "dvdbchar/Render/Window.hpp"
#include "dvdbchar/Model.hpp"
#include "dvdbchar/Utils.hpp"

#include <exec/static_thread_pool.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
};

using namespace dvdbchar::Render;
using namespace dvdbchar;

int main1() {
	try {
		auto model = Model { "public/VRM1_Constraint_Twist_Sample.vrm" };
		model.introduce_self();
	} catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
	return 0;
}

int main() {
	try {
		auto context = WgpuContext::global();
		auto pool	 = exec::static_thread_pool {};
		auto sched	 = context.get_scheduler_from(pool.get_scheduler());

		//
		auto cam = Camera {
			.position  = { 0., 0.,  1. },
			.direction = { 0., 0., -2. },
		};

		const auto global	 = get_mapping<GlobalRefl>("global", "shaders/Uniform.refl.json");
		auto	   global_ub = ReflectedUniformBuffer<GlobalRefl> { global };
		// clang-format off
		auto	   global_bg = Bindgroup {{
			.layout = layout<GlobalRefl>("global", "shaders/Uniform.refl.json"),
			.entries = std::array { wgpu::BindGroupEntry {
				.binding = 0,
				.buffer  = global_ub,
				.offset  = global.offset,
				.size	   = global.size,
			}}, 
		}};
		// clang-format on

		const auto camera	 = get_mapping<CameraRefl>("camera", "shaders/Uniform.refl.json");
		auto	   camera_ub = ReflectedUniformBuffer<CameraRefl> { camera };
		camera_ub.write(camera.view_matrix, cam.view_matrix());
		camera_ub.write(camera.projection_matrix, cam.projection_matrix());
		auto camera_bg = Bindgroup {
			{
				.layout	 = layout<CameraRefl>("camera", "shaders/Uniform.refl.json"),
				.entries = std::array { wgpu::BindGroupEntry {
					.binding = 0,
					.buffer	 = camera_ub,
					.offset	 = camera.offset,
					.size	 = camera.size,
				} },
			 }
		};

		auto window = Window {
			{
				.width	= 1920,
				.height = 1080,
				.title	= "dvdbchar",
			 }
		};
		cam.aspect = window.aspect();

		struct EscExiter {
			const Window& window;

			//
			auto operator()(Window::on_key_t, const KeySignal& key) const {
				if (key.key == GLFW_KEY_ESCAPE && key.action == GLFW_RELEASE)
					glfwSetWindowShouldClose(window.window(), GLFW_TRUE);
			}
		};

		auto _ = window.bind(
			Window::on_key(	 //
				FpsCameraController { cam },
				EscExiter { window }
			),
			Window::on_window_resize(  //
				CameraAspectAdaptor { cam }
			),
			Window::on_mouse_moved(	 //
				FpsCameraController { cam }
			)
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
		auto pipeline = Pipeline {
			context,
			Pipeline::Spec {
				.shader			   = *read_text_from("shaders/Pipeline.wgsl"),
				.format			   = window.format(),
				.bindgroup_layouts = std::array {
					layout<GlobalRefl>("global", "shaders/Uniform.refl.json"),
					layout<CameraRefl>("camera", "shaders/Uniform.refl.json"),
					// layout<PbrRefl>("pbr", "shaders/Uniform.refl.json"),
					// layout<ModelDataRefl>("model_data", "shaders/Uniform.refl.json"),
				}, 
			}
		};
		// clang-format on

		// auto graph = []() {
		// 	RenderGraphBuilder builder;

		// 	auto			   tex_present = builder.present_texture();

		// 	return builder.build_runtime(
		// 		BasePass {

		// 			.tex_target = tex_present,
		// 		}
		// 	);
		// }();

		std::jthread render { [&]() {
			while (!glfwWindowShouldClose(window.window())) {
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
				pass.SetPipeline(pipeline);
				pass.SetVertexBuffer(0, vb);
				pass.SetIndexBuffer(ib, wgpu::IndexFormat::Uint32);
				pass.SetBindGroup(0, global_bg);
				pass.SetBindGroup(1, camera_bg);
				// pass.Draw(3);
				pass.DrawIndexed(6);
				pass.End();
				wgpu::CommandBuffer commands = encoder.Finish();
				context.device.GetQueue().Submit(1, &commands);

				camera_ub.write(camera.view_matrix, cam.view_matrix());
				camera_ub.write(camera.projection_matrix, cam.projection_matrix());

				window.surface().Present();
				context.instance.ProcessEvents();
			}
		} };

		while (!glfwWindowShouldClose(window.window())) glfwPollEvents();

	} catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
	return 0;
}