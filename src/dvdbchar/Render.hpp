#pragma once

#include "dvdbchar/Render/Buffer.hpp"
#include "dvdbchar/ComptimeJson.hpp"
#include "dvdbchar/ShaderReflection.hpp "
#include "dvdbchar/Utils.hpp"
#include "dvdbchar/Glfw.hpp"
#include "webgpu/webgpu_cpp.h"

#include <dawn/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <stdexec/execution.hpp>
#include <exec/async_scope.hpp>
#include <exec/env.hpp>
#include <exec/static_thread_pool.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <simdjson.h>
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>

#include <stdexcept>
#include <functional>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <span>
#include <vector>

namespace dvdbchar::Render {
	class EnchancedDevice : public wgpu::Device {
	public:
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
		Field<glm::vec3> position;
		Field<glm::vec3> direction;
	};

	template<>
	struct ReflectionRegistry<CameraRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {	"position",	&CameraRefl::position },
				std::pair { "direction", &CameraRefl::direction },
			};
		}
	};

	struct PbrRefl {
		Field<glm::vec4> base_color;
		Field<float>	 metallic;
		Field<float>	 roughness;
		Field<float>	 alpha_cutoff;
	};

	template<>
	struct ReflectionRegistry<PbrRefl> {
		inline static consteval auto mapping() {
			return std::tuple {
				std::pair {	"base_color",	  &PbrRefl::base_color },
				std::pair {		"metallic",		&PbrRefl::metallic },
				std::pair {	"roughness",	 &PbrRefl::roughness },
				std::pair { "alpha_cutoff", &PbrRefl::alpha_cutoff },
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

	struct WindowSpec {
		int			width  = 800;
		int			height = 600;
		std::string title;
	};

	template<typename T>
	class PipelineExecution;

	class ComplexPipeline {
	public:
		ComplexPipeline() : ComplexPipeline({}, {}) {}

		ComplexPipeline(WindowSpec window_spec = {}, const WgpuContext::Spec& context_spec = {}) :

			_ctx(WgpuContext::create(context_spec)), _window_spec(std::move(window_spec)) {}

		~ComplexPipeline() {
			glfwDestroyWindow(_window);
			_as.request_stop();
			stdexec::sync_wait(_as.on_empty());
		}

	public:
		struct Vertice {
			glm::vec3					 pos;
			glm::vec3					 normal;
			glm::vec2					 uv;
			size_t						 tex_id;

			inline static constexpr auto vertex_attrib() noexcept {
				return std::to_array<wgpu::VertexAttribute>({
					{
						.format			= wgpu::VertexFormat::Float32x3,
						.offset			= 0,
						.shaderLocation = 0,
					 },
					{
						.format			= wgpu::VertexFormat::Float32x3,
						.offset			= 0,
						.shaderLocation = 1,
					 },
					{
						.format			= wgpu::VertexFormat::Float32x2,
						.offset			= 0,
						.shaderLocation = 2,
					 },
					{
						.format			= wgpu::VertexFormat::Uint32,
						.offset			= 0,
						.shaderLocation = 3,
					 },
				});
			}
		};

	private:
		inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
		};

		auto _init_window() {
			GlfwManager::init();

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			_window = glfwCreateWindow(
				_window_spec.width,
				_window_spec.height,
				_window_spec.title.data(),
				nullptr,
				nullptr
			);
			if (!_window)
				panic("[GLFW]: failed to create window!");

			glfwSetWindowUserPointer(_window, this);
			// glfwSetWindowSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
			// 	auto& self = *static_cast<Pipeline*>(glfwGetWindowUserPointer(window));
			// 	const wgpu::SurfaceConfiguration surface_conf = {
			// 		.device = self._ctx.device,
			// 		.format = self._format,
			// 		.width	= static_cast<uint32_t>(width),
			// 		.height = static_cast<uint32_t>(height),
			// 		// .presentMode = wgpu::PresentMode::Mailbox,
			// 		.presentMode = wgpu::PresentMode::Fifo,
			// 		// .presentMode = wgpu::PresentMode::Immediate,
			// 	};
			// 	self._surface.Unconfigure();
			// 	self._surface.Configure(&surface_conf);
			// });

			_surface = wgpu::glfw::CreateSurfaceForWindow(_ctx.instance, _window);

			wgpu::SurfaceCapabilities capabilities;
			_surface.GetCapabilities(_ctx.adapter, &capabilities);
			_format = capabilities.formats[0];

			//
			const wgpu::SurfaceConfiguration surface_conf = {
				.device = _ctx.device,
				.format = _format,
				.width	= static_cast<uint32_t>(_window_spec.width),
				.height = static_cast<uint32_t>(_window_spec.height),
				// .presentMode = wgpu::PresentMode::Mailbox,
				// .presentMode = wgpu::PresentMode::Fifo,
				// .presentMode = wgpu::PresentMode::Immediate,
				.presentMode = capabilities.presentModes[0],
			};
			_surface.Configure(&surface_conf);
		}

		auto _init_pipeline() {
			const wgpu::ShaderSourceWGSL	   wgsl { { .code = shader } };

			const wgpu::ShaderModuleDescriptor shader_module_desc = { .nextInChain = &wgsl };
			const wgpu::ShaderModule		   shader_module =
				_ctx.device.CreateShaderModule(&shader_module_desc);

			const wgpu::ColorTargetState color_target_state = {
				.format = _format,
			};
			const wgpu::FragmentState fragment_state = {
				.module		 = shader_module,
				.targetCount = 1,
				.targets	 = &color_target_state,
			};

			constexpr auto				   vertex_attrib = Vertice::vertex_attrib();
			const wgpu::VertexBufferLayout vertex_layout = {
				.stepMode		= wgpu::VertexStepMode::Vertex,
				.arrayStride	= sizeof(Vertice),
				.attributeCount = vertex_attrib.size(),
				.attributes		= vertex_attrib.data(),
			};

			const wgpu::RenderPipelineDescriptor pipeline_desc = {
						.layout = [&](){
							const wgpu::PipelineLayoutDescriptor desc = {
								//
							};
							return _ctx.device.CreatePipelineLayout(&desc);
						}(),
						.vertex	  = { 
							.module = shader_module, 
							.bufferCount = 1, 
							.buffers = &vertex_layout, 
						},
						.fragment = &fragment_state,
					};
			_pipeline				  = _ctx.device.CreateRenderPipeline(&pipeline_desc);

			_vertex					  = { _ctx.device, 3 };

			std::vector<Vertice> data = {
				Vertice { .pos = { .0, -.5, .0 } },
				Vertice { .pos = { .5, .5, .0 } },
				Vertice { .pos = { -.5, .5, .0 } },
			};
			std::vector<Vertice> data2 = {
				Vertice { .pos = { .0, .5, 0. } },
				Vertice { .pos = { .5, -.5, .0 } },
				Vertice { .pos = { -.5, -.5, .0 } },
			};

			stdexec::sync_wait(
				stdexec::starts_on(_ctx.get_scheduler(), _vertex->write_buffer(data))
			);
			stdexec::sync_wait(
				stdexec::starts_on(_ctx.get_scheduler(), _vertex->write_buffer(data2))
			);

			_dyn_buffer.emplace(_ctx);
		}

		void render() {
			wgpu::SurfaceTexture tex;
			_surface.GetCurrentTexture(&tex);

			// _vertex.async_write(
			// 	std::vector {
			// 		Vertice { .pos = { .0, -.5, .0 } },
			// 		Vertice { .pos = { .5, .5, .0 } },
			// 		Vertice { .pos = { -.5, .5, .0 } },
			// 	}
			// );

			// static auto data = std::array {
			// 	Vertice { .pos = { .0, -.5, .0 } },
			// 	Vertice { .pos = { .5, .5, .0 } },
			// 	Vertice { .pos = { -.5, .5, .0 } },
			// };
			// static auto time = std::chrono::high_resolution_clock::now();
			// data[0].pos.x =
			// 	((std::chrono::high_resolution_clock::now() - time).count() % 1000) * .001f;

			// _queue.WriteBuffer(_vertex.get(), 0, data.data(), data.size() * sizeof(Vertice));
			// _vertex.async_write(data);

			const wgpu::RenderPassColorAttachment attachment {
				.view	 = tex.texture.CreateView(),
				.loadOp	 = wgpu::LoadOp::Clear,
				.storeOp = wgpu::StoreOp::Store,
			};

			const wgpu::RenderPassDescriptor renderpass {
				.colorAttachmentCount = 1,
				.colorAttachments	  = &attachment,
			};

			wgpu::CommandEncoder	encoder = _ctx.device.CreateCommandEncoder();
			wgpu::RenderPassEncoder pass	= encoder.BeginRenderPass(&renderpass);
			pass.SetPipeline(_pipeline);
			pass.SetVertexBuffer(0, _vertex->get());
			pass.Draw(3);
			pass.End();
			wgpu::CommandBuffer commands = encoder.Finish();
			_ctx.device.GetQueue().Submit(1, &commands);
		}

	public:
		template<stdexec::scheduler Sch>
		auto get_scheduler_from(Sch&& sched) {
			return _ctx.get_scheduler_from(std::forward<Sch>(sched));
		}

		auto launch() noexcept {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([&](auto&& sched) {
					_init_window();
					_init_pipeline();

					spdlog::info("into loop");

					while (!glfwWindowShouldClose(_window)) {
						glfwPollEvents();
						render();
						_surface.Present();
						_ctx.instance.ProcessEvents();
					}
				});
		}

		auto context() const -> const WgpuContext& { return _ctx; }

		template<typename T>
		auto render_on(const T& resource) -> PipelineExecution<T> {
			return { *this, resource };
		}

	protected:
		WgpuContext							 _ctx;
		exec::async_scope					 _as;

		GLFWwindow*							 _window;
		WindowSpec							 _window_spec;
		wgpu::TextureFormat					 _format;
		wgpu::Surface						 _surface;

		wgpu::RenderPipeline				 _pipeline;

		std::optional<VertexBuffer<Vertice>> _vertex;
		std::optional<DynBuffer<wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>>
			_dyn_buffer;
	};

	template<typename T>
	class PipelineExecution {
	public:
		PipelineExecution(const ComplexPipeline& pipeline, const T& resources) :
			_pipeline(pipeline), _resources(resources) {}

	private:
		const ComplexPipeline& _pipeline;
		const T&			   _resources;
	};

}  // namespace dvdbchar::Render
