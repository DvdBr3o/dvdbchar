#pragma once

#include "dvdbchar/Glfw.hpp"
#include "stdexec/__detail/__completion_signatures.hpp"
#include "stdexec/__detail/__execution_fwd.hpp"
#include "stdexec/__detail/__senders_core.hpp"
#include "stdexec/__detail/__then.hpp"

#include <spdlog/spdlog.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_cpp_print.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_glfw.h>
#include <chrono>
#include <exception>
#include <glm/glm.hpp>
#include <memory>
#include <stdexec/execution.hpp>
#include <stdexec/coroutine.hpp>
#include <exec/task.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <span>
#include <utility>

namespace dvdbchar {
	using exec::task;

	struct Camera {
		glm::vec3 pos;
		glm::vec3 dir;
	};

	class EnchancedDevice : public wgpu::Device {
	public:
	};

	struct WgpuConctextScheduler {
		const wgpu::Instance& instance;
		const wgpu::Device&	  device;
		const wgpu::Queue&	  queue;

		struct query_instance_t {};

		struct query_device_t {};

		struct query_queue_t {};

		inline static constexpr auto query_instance = query_instance_t {};
		inline static constexpr auto query_device	= query_device_t {};
		inline static constexpr auto query_queue	= query_queue_t {};

		struct Env {
			const WgpuConctextScheduler& ctx;

			[[nodiscard]] auto& query(query_instance_t) const noexcept { return ctx.instance; }

			[[nodiscard]] auto& query(query_device_t) const noexcept { return ctx.device; }

			[[nodiscard]] auto& query(query_queue_t) const noexcept { return ctx.queue; }

			[[nodiscard]] const WgpuConctextScheduler& query(
				stdexec::get_scheduler_t
			) const noexcept {
				return ctx;
			}
		};

		using sender_concept		= stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures<
			stdexec::set_value_t(),					  //
			stdexec::set_error_t(std::exception_ptr)  //
			>;

		template<stdexec::receiver R>
		struct Opstate {
			R	 r;

			auto start() noexcept { stdexec::set_value(r); }
		};

		auto get_env() noexcept -> Env { return { *this }; }

		template<stdexec::receiver R>
		auto connect(R&& r) {
			return Opstate<R> { std::forward<R>(r) };
		}

		auto			   schedule() noexcept { return *this; }

		inline friend auto operator==(
			const WgpuConctextScheduler& lhs, const WgpuConctextScheduler& rhs
		) -> bool {
			return lhs.device.Get() == rhs.device.Get()		 //
				&& lhs.instance.Get() == rhs.instance.Get()	 //
				&& lhs.queue.Get() == rhs.queue.Get();
		}
	};

	template<typename T, wgpu::BufferUsage usage>
	class Buffer {
	public:
		class WriteGuard {
		public:
			friend class Buffer;

			~WriteGuard() { _buffer.Unmap(); }

			auto& operator*() { return _data; }

			auto* operator->() { return &_data; }

		private:
			WriteGuard(T& data, wgpu::Buffer& buffer) : _data(data), _buffer(buffer) {}

			WriteGuard(void* data, wgpu::Buffer& buffer) :
				_data(*static_cast<T*>(data)), _buffer(buffer) {}

		private:
			T&			  _data;
			wgpu::Buffer& _buffer;
		};

		struct WriteBufferSender {
			const Buffer&	   self;
			std::span<const T> data;

			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(),					  //
				stdexec::set_error_t(std::exception_ptr)  //
				>;

			template<stdexec::receiver R>
			struct Opstate {
				R				   r;
				const Buffer&	   buffer;
				std::span<const T> data;

				//
				auto start() noexcept {
					auto env   = stdexec::get_env(r);
					auto sched = stdexec::get_scheduler(env);

					// sched.queue
					// 	.WriteBuffer(buffer.get(), 0, static_cast<void*>(data.data()), data.size());

					stdexec::set_value(r);
				}
			};

			template<stdexec::receiver R>
			auto connect(R&& r) {
				return Opstate<R> { std::forward<R>(r), self, data };
			}
		};

		template<wgpu::MapMode mode>
		struct MapRequestSender {
			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(wgpu::MapAsyncStatus),	 //
				stdexec::set_error_t(std::exception_ptr),	 //
				stdexec::set_stopped_t()					 //
				>;

			const wgpu::Buffer& buffer;
			size_t				offset = 0;
			size_t				size   = sizeof(T);

			template<stdexec::receiver_of<completion_signatures> R>
			struct opstate {
				R					r;
				const wgpu::Buffer& buffer;
				size_t				offset = 0;
				size_t				size   = sizeof(T);

				//
				auto start() noexcept {
					buffer.MapAsync(
						mode,
						offset,
						size,
						wgpu::CallbackMode::AllowProcessEvents,
						[&](wgpu::MapAsyncStatus status, wgpu::StringView) {
							if (status == wgpu::MapAsyncStatus::Success)
								stdexec::set_value(r, status);
							else
								stdexec::set_error(
									r,
									std::make_exception_ptr(std::runtime_error { "async fail" })
								);
						}
					);
				}
			};

			template<stdexec::receiver Receiver>
			void submit(Receiver&& receiver) const {
				auto op = connect(std::forward<Receiver>(receiver));
				stdexec::start(op);
			}

			constexpr auto connect(stdexec::receiver_of<completion_signatures> auto&& r) const {
				return opstate { std::forward<decltype(r)>(r), buffer, offset, size };
			}
		};

		using MapReadRequestSender	= MapRequestSender<wgpu::MapMode::Read>;
		using MapWriteRequestSender = MapRequestSender<wgpu::MapMode::Write>;

	public:
		Buffer() = default;

		Buffer(const wgpu::Device& device, size_t size = 1) : _size(size) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = _size * sizeof(T),
				.mappedAtCreation = false,
			};
			_buffer = device.CreateBuffer(&buffer_desc);
		}

		friend auto write_buffer(Buffer& self, wgpu::Queue& queue, const T& data) {
			queue.WriteBuffer(self, 0, &data, sizeof(T));
		}

		auto write_buffer(std::span<const T> data) { return WriteBufferSender { *this, data }; }

		template<wgpu::BufferUsage another_usage>
		friend auto bufcpy(
			const wgpu::Device& device, const wgpu::Queue& queue, const Buffer& dst,
			const Buffer<T, another_usage>& src
		) {
			auto encoder = device.CreateCommandEncoder();
			encoder.CopyBufferToBuffer(src.get(), 0, dst.get(), 0, src.size() * sizeof(T));
			const auto cmd = encoder.Finish();
			queue.Submit(1, &cmd);
		}

		// auto async_read() const {
		// 	static_assert(
		// 		usage & wgpu::BufferUsage::MapWrite,
		// 		"Buffer cannot be `async_read`ed unless it contains `wgpu::BufferUsage::MapRead`!"
		// 	);
		// 	return MapReadRequestSender { _buffer };
		// }

		auto async_write() {
			static_assert(
				usage & wgpu::BufferUsage::MapWrite,
				"Buffer cannot be `async_write`ed unless it contains `wgpu::BufferUsage::MapWrite`!"
			);

			using namespace stdexec;
			return MapWriteRequestSender { _buffer, 0, size() * sizeof(T) } | then([&](auto&&...) {
					   return WriteGuard { _buffer.GetMappedRange(), _buffer };
				   });
		}

		auto async_write(std::invocable<T&> auto&& f) -> wgpu::Future {
			static_assert(
				usage & wgpu::BufferUsage::MapWrite,
				"Buffer cannot be `async_write`ed unless it contains `wgpu::BufferUsage::MapWrite`!"
			);

			return _buffer.MapAsync(
				wgpu::MapMode::Write,
				0,
				size() * sizeof(T),
				wgpu::CallbackMode::AllowProcessEvents,
				[&,
				 f = std::forward<decltype(f)>(f)](wgpu::MapAsyncStatus status, wgpu::StringView) {
					if (status == wgpu::MapAsyncStatus::Success) {
						std::invoke(f, *static_cast<T*>(_buffer.GetMappedRange()));
						_buffer.Unmap();
					} else
						throw std::runtime_error {
							std::format("Buffer map async failed, status = {}!", (int)status)
						};
				}
			);
		}

		auto async_write_no_unmap(std::invocable<T&> auto&& f) -> wgpu::Future {
			static_assert(
				usage & wgpu::BufferUsage::MapWrite,
				"Buffer cannot be `async_write`ed unless it contains `wgpu::BufferUsage::MapWrite`!"
			);

			return _buffer.MapAsync(
				wgpu::MapMode::Write,
				0,
				size() * sizeof(T),
				wgpu::CallbackMode::AllowProcessEvents,
				[&,
				 f = std::forward<decltype(f)>(f)](wgpu::MapAsyncStatus status, wgpu::StringView) {
					if (status == wgpu::MapAsyncStatus::Success) {
						std::invoke(f, *static_cast<T*>(_buffer.GetMappedRange()));
						_buffer.Unmap();
					} else
						throw std::runtime_error {
							std::format("Buffer map async failed, status = {}!", (int)status)
						};
				}
			);
		}

		auto async_write(T&& data) -> wgpu::Future {
			static_assert(
				usage & wgpu::BufferUsage::MapWrite,
				"Buffer cannot be `async_write`ed unless it contains `wgpu::BufferUsage::MapWrite`!"
			);

			return _buffer.MapAsync(
				wgpu::MapMode::Write,
				0,
				size() * sizeof(T),
				wgpu::CallbackMode::AllowProcessEvents,
				[&](wgpu::MapAsyncStatus status, wgpu::StringView) {
					if (status == wgpu::MapAsyncStatus::Success) {
						*static_cast<T*>(_buffer.GetMappedRange()) = std::forward<T>(data);
						_buffer.Unmap();
					} else
						throw std::runtime_error {
							std::format("Buffer map async failed, status = {}!", (int)status)
						};
				}
			);
		}

		auto			   get() -> wgpu::Buffer& { return _buffer; }

		auto			   get() const -> const wgpu::Buffer& { return _buffer; }

		auto&			   operator*() { return _buffer; }

		const auto&		   operator*() const { return _buffer; }

		auto*			   operator->() { return &_buffer; }

		const auto*		   operator->() const { return &_buffer; }

		[[nodiscard]] auto size() const -> size_t { return _size; }

	protected:
		wgpu::Buffer _buffer = nullptr;
		size_t		 _size	 = 1;
	};

	template<typename T>
	using VertexBuffer = Buffer<T, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>;
	template<typename T>
	using IndexBuffer = Buffer<T, wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst>;
	template<typename T>
	using StagingBuffer = Buffer<T, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc>;

	template<typename T, wgpu::BufferUsage usage>
		requires(((uint32_t)usage & (uint32_t)wgpu::BufferUsage::CopyDst) != 0)
	class StagedBuffer : protected Buffer<T, usage> {
	public:
		using OriginalBufferT = Buffer<T, usage>;

	public:
		StagedBuffer() = default;

		StagedBuffer(const wgpu::Device& device, size_t size = 1) :
			_device(&device), OriginalBufferT(device, size), _staging(device, size) {}

	public:
		auto async_write(std::invocable<T&> auto&& f) {
			if (!_state->_available) {
				_state->_cancelling = true;
				_staging.get().Unmap();
			}
			_state->_available = false;
			_staging.async_write_no_unmap([&, f = std::forward<decltype(f)>(f)](T& data) {
				if (_state->_cancelling) {
					_state->_available	= true;
					_state->_cancelling = false;
					return;
				}
				std::invoke(std::forward<decltype(f)>(f), data);
				_staging.get().Unmap();

				bufcpy(
					*_device,
					_device->GetQueue(),
					static_cast<OriginalBufferT&>(*this),
					_staging
				);
				_state->_available = true;
			});
		}

		auto async_write(T&& data) {
			if (!_state->_available) {
				_state->_cancelling = true;
				_staging.get().Unmap();
			}
			_state->_available = false;
			_staging.async_write_no_unmap([&, data = std::forward<T>(data)](T& d) {
				if (_state->_cancelling) {
					_state->_available	= true;
					_state->_cancelling = false;
					return;
				}
				d = std::move(data);
				_staging.get().Unmap();

				bufcpy(
					*_device,
					_device->GetQueue(),
					static_cast<OriginalBufferT&>(*this),
					_staging
				);
				_state->_available = true;
			});
		}

		auto async_write(std::span<const T> data) {
			if (!_state->_available) {
				_state->_cancelling = true;
				_staging.get().Unmap();
			}
			_state->_available = false;
			_staging.get().MapAsync(
				wgpu::MapMode::Write,
				0,
				data.size() * sizeof(T),
				wgpu::CallbackMode::AllowProcessEvents,
				[&, data](wgpu::MapAsyncStatus status, wgpu::StringView) {
					if (_state->_cancelling) {
						_state->_available	= true;
						_state->_cancelling = false;
						return;
					}
					void* p = _staging.get().GetMappedRange();
					std::memcpy(p, data.data(), data.size() * sizeof(T));
					_staging.get().Unmap();

					bufcpy(
						*_device,
						_device->GetQueue(),
						static_cast<OriginalBufferT&>(*this),
						_staging
					);
					_state->_available = true;
				}
			);
		}

		auto async_write(std::vector<T>&& data) {
			if (!_state->_available) {
				_state->_cancelling = true;
				_staging.get().Unmap();
			}
			_state->_available = false;
			_staging.get().MapAsync(
				wgpu::MapMode::Write,
				0,
				_staging.size() * sizeof(T),
				wgpu::CallbackMode::AllowProcessEvents,
				[&, data = std::move(data)](wgpu::MapAsyncStatus status, wgpu::StringView) {
					if (_state->_cancelling) {
						_state->_available	= true;
						_state->_cancelling = false;
						return;
					}
					void* p = _staging.get().GetMappedRange();
					std::memcpy(p, data.data(), data.size() * sizeof(T));
					_staging.get().Unmap();

					spdlog::info("writing:");
					for (const auto& d : data)
						spdlog::info("{}, {}, {}", d.pos.x, d.pos.y, d.pos.z);

					bufcpy(
						*_device,
						_device->GetQueue(),
						static_cast<OriginalBufferT&>(*this),
						_staging
					);
					_state->_available = true;
				}
			);
		}

		using OriginalBufferT::get;
		using OriginalBufferT::write_buffer;
		using OriginalBufferT::operator*;
		using OriginalBufferT::operator->;

	protected:
		struct State {
			std::atomic<bool> _available  = true;
			std::atomic<bool> _cancelling = false;
		};

	protected:
		const wgpu::Device*	   _device;
		StagingBuffer<T>	   _staging;
		std::unique_ptr<State> _state = std::make_unique<State>(true, false);
	};

	template<typename T>
	using StagedVertexBuffer =
		StagedBuffer<T, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>;

	class Pipeline {
	public:
		struct Vertice {
			glm::vec3					 pos;
			glm::vec3					 normal;
			glm::vec2					 uv;
			size_t						 tex_id;

			inline static constexpr auto vertex_attrib() {
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

		struct Spec {
			int			width;
			int			height;
			std::string title;
		};

		Pipeline() : Pipeline(Spec {}) {}

		Pipeline(const Spec& spec);

	public:
		inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
		};

	public:
		void launch();
		void render();

	private:
		void _init_context();
		void _init_window(const Spec& spec);
		void _init_pipeline();

	private:
		Spec						_spec;
		wgpu::Instance				_instance;
		wgpu::Adapter				_adapter;
		wgpu::Device				_device;
		wgpu::Queue					_queue;
		GLFWwindow*					_window;
		wgpu::Surface				_surface;
		wgpu::TextureFormat			_format;
		wgpu::RenderPipeline		_pipeline;

		StagedVertexBuffer<Vertice> _vertex;
	};
}  // namespace dvdbchar

namespace dvdbchar {
	inline Pipeline::Pipeline(const Spec& spec) : _spec(spec) {
		_init_context();
	}

	inline void Pipeline::_init_context() {
		const wgpu::InstanceDescriptor instance_desc = {
            .capabilities = {
                .timedWaitAnyEnable = true,
            },
        };
		_instance									  = wgpu::CreateInstance(&instance_desc);

		const wgpu::RequestAdapterOptions adapter_opt = {
			// .backendType = wgpu::BackendType::Vulkan,
		};
		_instance.WaitAny(
			_instance.RequestAdapter(
				&adapter_opt,
				wgpu::CallbackMode::WaitAnyOnly,
				[&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView message) {
					if (status == wgpu::RequestAdapterStatus::Success) {
						_adapter = std::move(a);
					} else {
						spdlog::critical(std::string_view(message));
						throw std::runtime_error { std::string(message) };
					}
				}
			),
			std::numeric_limits<uint32_t>::max()
		);

		wgpu::DeviceDescriptor device_desc {};
		device_desc.SetUncapturedErrorCallback(
			[](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
				spdlog::error("[WGPU/{}]: {}", (int)errorType, std::string_view(message));
			}
		);
		_instance.WaitAny(
			_adapter.RequestDevice(
				&device_desc,
				wgpu::CallbackMode::WaitAnyOnly,
				[&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView message) {
					if (status == wgpu::RequestDeviceStatus::Success) {
						_device = std::move(d);
					} else {
						spdlog::critical(std::string_view(message));
						throw std::runtime_error { std::string(message) };
					}
				}
			),
			std::numeric_limits<uint32_t>::max()
		);
		_device.SetLoggingCallback([](wgpu::LoggingType type, wgpu::StringView message) {
			switch (type) {
				case wgpu::LoggingType::Verbose: spdlog::trace(std::string_view(message)); break;
				case wgpu::LoggingType::Info: spdlog::info(std::string_view(message)); break;
				case wgpu::LoggingType::Warning: spdlog::warn(std::string_view(message)); break;
				case wgpu::LoggingType::Error: spdlog::critical(std::string_view(message)); break;
			}
		});

		_queue = _device.GetQueue();

		// _vertex = ResiableStagedVertexBuffer<Vertice>::create(_device, 3);

		// _vertex->async_write(
		// 	std::vector {
		// 		Vertice { .pos = { .0, -.5, .0 } },
		// 		Vertice { .pos = { .5, .5, .0 } },
		// 		Vertice { .pos = { -.5, .5, .0 } },
		// 	}
		// );
		_vertex = std::move(StagedVertexBuffer<Vertice> { _device, 3 });

		_vertex.async_write(
			std::vector {
				Vertice { .pos = { .0, -.5, .0 } },
				Vertice { .pos = { .5, .5, .0 } },
				Vertice { .pos = { -.5, .5, .0 } },
			}
		);
	}

	inline void Pipeline::_init_window(const Spec& spec) {
		GlfwManager::init();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(spec.width, spec.height, spec.title.data(), nullptr, nullptr);
		if (!_window) {
			spdlog::error("glfw failed to create window!");
			throw std::runtime_error { "glfw failed to create window!" };
		}

		_surface = wgpu::glfw::CreateSurfaceForWindow(_instance, _window);

		wgpu::SurfaceCapabilities capabilities;
		_surface.GetCapabilities(_adapter, &capabilities);
		_format = capabilities.formats[0];

		//
		const wgpu::SurfaceConfiguration surface_conf = {
			.device		 = _device,
			.format		 = _format,
			.width		 = static_cast<uint32_t>(spec.width),
			.height		 = static_cast<uint32_t>(spec.height),
			.presentMode = wgpu::PresentMode::Mailbox,
		};
		_surface.Configure(&surface_conf);

		// glfwSetWindowSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
		// 	auto& self = *static_cast<Pipeline*>(glfwGetWindowUserPointer(window));
		// 	const wgpu::SurfaceConfiguration surface_conf = {
		// 		.device		 = self._device,
		// 		.format		 = self._format,
		// 		.width		 = static_cast<uint32_t>(width),
		// 		.height		 = static_cast<uint32_t>(height),
		// 		.presentMode = wgpu::PresentMode::Mailbox,
		// 	};
		// 	self._surface.Configure(&surface_conf);
		// });
		// glfwSetWindowUserPointer(_window, this);
	}

	inline void Pipeline::_init_pipeline() {
		const wgpu::ShaderSourceWGSL	   wgsl { { .code = shader } };

		const wgpu::ShaderModuleDescriptor shader_module_desc = { .nextInChain = &wgsl };
		const wgpu::ShaderModule shader_module = _device.CreateShaderModule(&shader_module_desc);

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
			.vertex	  = { 
				.module = shader_module, 
				.bufferCount = 1, 
				.buffers = &vertex_layout, 
			},
			.fragment = &fragment_state,
		};
		_pipeline = _device.CreateRenderPipeline(&pipeline_desc);
	}

	inline void Pipeline::launch() {
		_init_window(_spec);
		_init_pipeline();

		while (!glfwWindowShouldClose(_window)) {
			glfwPollEvents();
			render();
			_surface.Present();
			_instance.ProcessEvents();
		}
	}

	inline void Pipeline::render() {
		wgpu::SurfaceTexture tex;
		_surface.GetCurrentTexture(&tex);

		// _vertex.async_write(
		// 	std::vector {
		// 		Vertice { .pos = { .0, -.5, .0 } },
		// 		Vertice { .pos = { .5, .5, .0 } },
		// 		Vertice { .pos = { -.5, .5, .0 } },
		// 	}
		// );

		static auto data = std::array {
			Vertice { .pos = { .0, -.5, .0 } },
			Vertice { .pos = { .5, .5, .0 } },
			Vertice { .pos = { -.5, .5, .0 } },
		};
		static auto time = std::chrono::high_resolution_clock::now();
		data[0].pos.x = ((std::chrono::high_resolution_clock::now() - time).count() % 1000) * .001f;

		// _queue.WriteBuffer(_vertex.get(), 0, data.data(), data.size() * sizeof(Vertice));
		_vertex.async_write(data);

		const wgpu::RenderPassColorAttachment attachment {
			.view	 = tex.texture.CreateView(),
			.loadOp	 = wgpu::LoadOp::Clear,
			.storeOp = wgpu::StoreOp::Store,
		};

		const wgpu::RenderPassDescriptor renderpass {
			.colorAttachmentCount = 1,
			.colorAttachments	  = &attachment,
		};

		wgpu::CommandEncoder	encoder = _device.CreateCommandEncoder();
		wgpu::RenderPassEncoder pass	= encoder.BeginRenderPass(&renderpass);
		pass.SetPipeline(_pipeline);
		pass.SetVertexBuffer(0, _vertex.get());
		pass.Draw(3);
		pass.End();
		wgpu::CommandBuffer commands = encoder.Finish();
		_device.GetQueue().Submit(1, &commands);
	}
}  // namespace dvdbchar