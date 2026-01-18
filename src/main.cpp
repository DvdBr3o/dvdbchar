// #include <iostream>

// #include <GLFW/glfw3.h>
// #include <dawn/webgpu_cpp_print.h>
// #include <webgpu/webgpu_cpp.h>
// #include <webgpu/webgpu_glfw.h>
// #include <slang.h>

// struct Context {
// 	wgpu::Instance		 instance;
// 	wgpu::Adapter		 adapter;
// 	wgpu::Device		 device;
// 	wgpu::RenderPipeline pipeline;

// 	wgpu::Surface		 surface;
// 	wgpu::TextureFormat	 format;
// 	const uint32_t		 kWidth	 = 1080;
// 	const uint32_t		 kHeight = 720;

// 	void				 ConfigureSurface() {
// 		wgpu::SurfaceCapabilities capabilities;
// 		surface.GetCapabilities(adapter, &capabilities);
// 		format = capabilities.formats[0];

// 		//
// 		const wgpu::SurfaceConfiguration config = {
// 							.device		 = device,
// 							.format		 = format,
// 							.width		 = kWidth,
// 							.height		 = kHeight,
// 							.presentMode = wgpu::PresentMode::Mailbox,
// 		};
// 		surface.Configure(&config);
// 	}

// 	void Init() {
// 		// static const auto		 kTimedWaitAny = {};
// 		wgpu::InstanceDescriptor instanceDesc {
// 			.capabilities = {
// 				.timedWaitAnyEnable = true,
// 			},
// 		};
// 		instance		= wgpu::CreateInstance(&instanceDesc);

// 		wgpu::Future f1 = instance.RequestAdapter(
// 			nullptr,
// 			wgpu::CallbackMode::WaitAnyOnly,
// 			[&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView message) {
// 				if (status != wgpu::RequestAdapterStatus::Success) {
// 					std::cout << "RequestAdapter: " << message << "\n";
// 					exit(0);
// 				}
// 				adapter = std::move(a);
// 			}
// 		);
// 		instance.WaitAny(f1, UINT64_MAX);

// 		wgpu::DeviceDescriptor desc {};
// 		desc.SetUncapturedErrorCallback(
// 			[](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
// 				std::cout << "Error: " << errorType << " - message: " << message << "\n";
// 			}
// 		);

// 		wgpu::Future f2 = adapter.RequestDevice(
// 			&desc,
// 			wgpu::CallbackMode::WaitAnyOnly,
// 			[&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView message) {
// 				if (status != wgpu::RequestDeviceStatus::Success) {
// 					std::cout << "RequestDevice: " << message << "\n";
// 					exit(0);
// 				}
// 				device = std::move(d);
// 			}
// 		);
// 		instance.WaitAny(f2, UINT64_MAX);
// 	}

// 	static constexpr char shaderCode[] = {
// #include "Pipeline.wgsl.h"
// 	};

// 	//
// 	void CreateRenderPipeline() {
// 		wgpu::ShaderSourceWGSL		 wgsl { { .code = shaderCode } };

// 		wgpu::ShaderModuleDescriptor shaderModuleDescriptor { .nextInChain = &wgsl };
// 		wgpu::ShaderModule	   shaderModule = device.CreateShaderModule(&shaderModuleDescriptor);

// 		wgpu::ColorTargetState colorTargetState { .format = format };

// 		wgpu::FragmentState	   fragmentState { .module		= shaderModule,
// 											   .targetCount = 1,
// 											   .targets		= &colorTargetState };

// 		wgpu::RenderPipelineDescriptor descriptor { .vertex	  = { .module = shaderModule },
// 													.fragment = &fragmentState };
// 		pipeline = device.CreateRenderPipeline(&descriptor);
// 	}

// 	void Render() {
// 		wgpu::SurfaceTexture surfaceTexture;
// 		surface.GetCurrentTexture(&surfaceTexture);

// 		wgpu::RenderPassColorAttachment attachment { .view	  = surfaceTexture.texture.CreateView(),
// 													 .loadOp  = wgpu::LoadOp::Clear,
// 													 .storeOp = wgpu::StoreOp::Store };

// 		wgpu::RenderPassDescriptor		renderpass { .colorAttachmentCount = 1,
// 													 .colorAttachments	   = &attachment };

// 		wgpu::CommandEncoder			encoder = device.CreateCommandEncoder();
// 		wgpu::RenderPassEncoder			pass	= encoder.BeginRenderPass(&renderpass);
// 		pass.SetPipeline(pipeline);
// 		pass.Draw(3);
// 		pass.End();
// 		wgpu::CommandBuffer commands = encoder.Finish();
// 		device.GetQueue().Submit(1, &commands);
// 	}

// 	void InitGraphics() {
// 		ConfigureSurface();
// 		CreateRenderPipeline();
// 	}

// 	void Start() {
// 		if (!glfwInit())
// 			return;

// 		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
// 		GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);
// 		surface			   = wgpu::glfw::CreateSurfaceForWindow(instance, window);
// 		// surface = wgpu::Surface { glfwCreateWindowWGPUSurface(instance.Get(), window) };

// 		InitGraphics();

// #if defined(__EMSCRIPTEN__)
// 		emscripten_set_main_loop(Render, 0, false);
// #else
// 		while (!glfwWindowShouldClose(window)) {
// 			glfwPollEvents();
// 			Render();
// 			surface.Present();
// 			instance.ProcessEvents();
// 		}
// #endif
// 	}
// };

// int main() {
// 	Context ctx;
// 	ctx.Init();
// 	ctx.Start();
// }

#include "dvdbchar/OscServer.hpp"
// #include "dvdbchar/Forwarding.hpp"
#include "dvdbchar/Pipeline.hpp"
#include "dvdbchar/Model.hpp"
#include "dvdbchar/Render.hpp"
#include "exec/async_scope.hpp"
#include "exec/task.hpp"
#include "stdexec/__detail/__domain.hpp"
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

#include <exec/static_thread_pool.hpp>

using namespace stdexec;
using namespace dvdbchar::Render;

int main() {
	try {
		WgpuContext ctx = WgpuContext::create();
		Render::Buffer<int, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc> buffer = {
			ctx.device,
			1
		};
		exec::static_thread_pool pool { 8 };

		auto					 sched = ctx.get_scheduler_from(pool.get_scheduler());
		// auto sched = ctx.get_scheduler();

		auto data = std::vector<int> { 1 };
		// auto			  work	= starts_on(sched, buffer.write_buffer(data));
		auto			  work2 = starts_on(sched, buffer.async_write(data));

		exec::async_scope as;
		std::stop_source  ss;

		as.spawn(work2 | then([](auto&&) { spdlog::info("after work2!"); }));

		as.spawn(when_all(
			starts_on(sched, just() | then([&]() {
								 spdlog::info("into sleep");
								 std::this_thread::sleep_for(std::chrono::seconds(3));
								 spdlog::info("after sleep");
								 ss.request_stop();
							 })),
			starts_on(sched, ctx.launch(ss.get_token()) | then([]() {
								 spdlog::info("after launch!");
							 }))
		));

		spdlog::info("after spawn");

		sync_wait(as.on_empty());
		spdlog::info("yep");
	} catch (const std::exception& e) { spdlog::error("Uncaught error: {}", e.what()); }
}
